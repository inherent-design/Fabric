#include "fabric/render/PostProcess.hh"

#include "fabric/core/Log.hh"
#include "fabric/render/Rendering.hh"
#include "fabric/utils/Profiler.hh"

// Vulkan-only: suppress all non-SPIR-V shader profiles so
// BGFX_EMBEDDED_SHADER only references *_spv symbol arrays.
#define BGFX_PLATFORM_SUPPORTS_DXBC 0
#define BGFX_PLATFORM_SUPPORTS_DXIL 0
#define BGFX_PLATFORM_SUPPORTS_ESSL 0
#define BGFX_PLATFORM_SUPPORTS_GLSL 0
#define BGFX_PLATFORM_SUPPORTS_METAL 0
#define BGFX_PLATFORM_SUPPORTS_NVN 0
#define BGFX_PLATFORM_SUPPORTS_PSSL 0
#define BGFX_PLATFORM_SUPPORTS_WGSL 0
#include <bgfx/embedded_shader.h>

// Compiled SPIR-V shader bytecode generated at build time from .sc sources.
#include "spv/fs_blur.sc.bin.h"
#include "spv/fs_bright.sc.bin.h"
#include "spv/fs_tonemap.sc.bin.h"
#include "spv/vs_fullscreen.sc.bin.h"

static const bgfx::EmbeddedShader g_s_postShaders[] = {BGFX_EMBEDDED_SHADER(vs_fullscreen),
                                                       BGFX_EMBEDDED_SHADER(fs_bright), BGFX_EMBEDDED_SHADER(fs_blur),
                                                       BGFX_EMBEDDED_SHADER(fs_tonemap), BGFX_EMBEDDED_SHADER_END()};

// Fullscreen triangle vertices in clip space.
static const float g_s_fullscreenVertices[] = {
    -1.0f, -1.0f, 0.0f, 3.0f, -1.0f, 0.0f, -1.0f, 3.0f, 0.0f,
};

namespace fabric {

PostProcess::PostProcess() = default;

PostProcess::~PostProcess() {
    shutdown();
}

void PostProcess::init(uint16_t width, uint16_t height) {
    if (initialized_) {
        return;
    }

    if (width == 0 || height == 0) {
        FABRIC_LOG_WARN("PostProcess::init called with zero dimensions ({}x{})", width, height);
        return;
    }

    width_ = width;
    height_ = height;

    initPrograms();
    if (!brightProgram_.isValid()) {
        return;
    }

    createFramebuffers(width, height);
    initialized_ = true;

    const auto& caps = renderCaps();
    FABRIC_LOG_INFO("PostProcess initialized: {}x{}, backend={}", width, height, caps.rendererName);
}

void PostProcess::shutdown() {
    destroyFramebuffers();

    samplerInputTex_.reset();
    samplerBloomTex_.reset();
    samplerHdrColor_.reset();
    uniformTonemapParams_.reset();
    uniformTexelSize_.reset();
    uniformBloomParams_.reset();
    vbh_.reset();
    tonemapProgram_.reset();
    blurProgram_.reset();
    brightProgram_.reset();

    width_ = 0;
    height_ = 0;
    initialized_ = false;
}

bool PostProcess::isValid() const {
    return initialized_ && brightProgram_.isValid();
}

void PostProcess::resize(uint16_t width, uint16_t height) {
    if (width == 0 || height == 0) {
        return;
    }

    if (width == width_ && height == height_) {
        return;
    }

    width_ = width;
    height_ = height;

    if (initialized_) {
        destroyFramebuffers();
        createFramebuffers(width, height);
        FABRIC_LOG_DEBUG("PostProcess resized to {}x{}", width, height);
    }
}

void PostProcess::setThreshold(float threshold) {
    threshold_ = threshold;
}

float PostProcess::threshold() const {
    return threshold_;
}

void PostProcess::setIntensity(float intensity) {
    intensity_ = intensity;
}

float PostProcess::intensity() const {
    return intensity_;
}

void PostProcess::setExposure(float exposure) {
    exposure_ = exposure;
}

float PostProcess::exposure() const {
    return exposure_;
}

bgfx::FrameBufferHandle PostProcess::hdrFramebuffer() const {
    return hdrFb_.get();
}

void PostProcess::setOutputTarget(bgfx::FrameBufferHandle fb) {
    outputTarget_ = fb;
}

void PostProcess::render(uint8_t baseViewId) {
    FABRIC_ZONE_SCOPED_N("PostProcess::render");

    if (!isValid()) {
        return;
    }

    // Pass 0: Brightness extraction
    {
        uint8_t view = baseViewId;
        bgfx::setViewName(view, "Bright Extract");
        bgfx::setViewRect(view, 0, 0, width_ / 2, height_ / 2);
        bgfx::setViewFrameBuffer(view, bloomFb_[0].get());
        bgfx::setViewClear(view, BGFX_CLEAR_COLOR, 0x000000ff, 1.0f, 0);

        float bloomParams[4] = {threshold_, threshold_ * 0.5f, 0.0f, 0.0f};
        bgfx::setUniform(uniformBloomParams_.get(), bloomParams);

        bgfx::setTexture(0, samplerHdrColor_.get(), bgfx::getTexture(hdrFb_.get(), 0));
        bgfx::setVertexBuffer(0, vbh_.get());
        bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
        bgfx::submit(view, brightProgram_.get());
    }

    // Passes 1..K_BLUR_PASSES-1: Dual Kawase blur (downsample chain)
    for (uint16_t i = 1; i < K_BLUR_PASSES; ++i) {
        uint8_t view = baseViewId + i;
        bgfx::setViewName(view, "Bloom Blur");

        uint16_t w = width_ >> (i + 1);
        uint16_t h = height_ >> (i + 1);
        if (w < 1)
            w = 1;
        if (h < 1)
            h = 1;

        bgfx::setViewRect(view, 0, 0, w, h);
        bgfx::setViewFrameBuffer(view, bloomFb_[i].get());
        bgfx::setViewClear(view, BGFX_CLEAR_COLOR, 0x000000ff, 1.0f, 0);

        float texelSize[4] = {1.0f / static_cast<float>(w), 1.0f / static_cast<float>(h), 0.0f, static_cast<float>(i)};
        bgfx::setUniform(uniformTexelSize_.get(), texelSize);

        bgfx::setTexture(0, samplerInputTex_.get(), bgfx::getTexture(bloomFb_[i - 1].get(), 0));
        bgfx::setVertexBuffer(0, vbh_.get());
        bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
        bgfx::submit(view, blurProgram_.get());
    }

    // Final pass: Tonemapping composite to backbuffer
    {
        uint8_t view = baseViewId + K_BLUR_PASSES;
        bgfx::setViewName(view, "Tonemap");
        bgfx::setViewRect(view, 0, 0, width_, height_);
        bgfx::setViewFrameBuffer(view, outputTarget_);
        bgfx::setViewClear(view, BGFX_CLEAR_NONE);

        float tonemapParams[4] = {intensity_, exposure_, 0.0f, 0.0f};
        bgfx::setUniform(uniformTonemapParams_.get(), tonemapParams);

        bgfx::setTexture(0, samplerHdrColor_.get(), bgfx::getTexture(hdrFb_.get(), 0));
        bgfx::setTexture(1, samplerBloomTex_.get(), bgfx::getTexture(bloomFb_[K_BLUR_PASSES - 1].get(), 0));
        bgfx::setVertexBuffer(0, vbh_.get());
        bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
        bgfx::submit(view, tonemapProgram_.get());
    }
}

void PostProcess::initPrograms() {
    bgfx::RendererType::Enum type = bgfx::getRendererType();

    brightProgram_.reset(bgfx::createProgram(bgfx::createEmbeddedShader(g_s_postShaders, type, "vs_fullscreen"),
                                             bgfx::createEmbeddedShader(g_s_postShaders, type, "fs_bright"), true));

    blurProgram_.reset(bgfx::createProgram(bgfx::createEmbeddedShader(g_s_postShaders, type, "vs_fullscreen"),
                                           bgfx::createEmbeddedShader(g_s_postShaders, type, "fs_blur"), true));

    tonemapProgram_.reset(bgfx::createProgram(bgfx::createEmbeddedShader(g_s_postShaders, type, "vs_fullscreen"),
                                              bgfx::createEmbeddedShader(g_s_postShaders, type, "fs_tonemap"), true));

    uniformBloomParams_.reset(bgfx::createUniform("u_bloomParams", bgfx::UniformType::Vec4));
    uniformTexelSize_.reset(bgfx::createUniform("u_texelSize", bgfx::UniformType::Vec4));
    uniformTonemapParams_.reset(bgfx::createUniform("u_tonemapParams", bgfx::UniformType::Vec4));
    samplerHdrColor_.reset(bgfx::createUniform("s_hdrColor", bgfx::UniformType::Sampler));
    samplerBloomTex_.reset(bgfx::createUniform("s_bloomTex", bgfx::UniformType::Sampler));
    samplerInputTex_.reset(bgfx::createUniform("s_inputTex", bgfx::UniformType::Sampler));

    // Fullscreen triangle vertex buffer
    bgfx::VertexLayout layout;
    layout.begin().add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float).end();
    vbh_.reset(bgfx::createVertexBuffer(bgfx::makeRef(g_s_fullscreenVertices, sizeof(g_s_fullscreenVertices)), layout));

    if (!brightProgram_.isValid() || !blurProgram_.isValid() || !tonemapProgram_.isValid() || !vbh_.isValid()) {
        FABRIC_LOG_ERROR("PostProcess shader init failed for renderer {}", bgfx::getRendererName(type));
        shutdown();
    }
}

void PostProcess::createFramebuffers(uint16_t width, uint16_t height) {
    // HDR scene framebuffer: RGBA16F color + D24S8 depth
    bgfx::TextureHandle hdrTextures[2];
    hdrTextures[0] = bgfx::createTexture2D(width, height, false, 1, bgfx::TextureFormat::RGBA16F, BGFX_TEXTURE_RT);
    hdrTextures[1] = bgfx::createTexture2D(width, height, false, 1, bgfx::TextureFormat::D24S8, BGFX_TEXTURE_RT);
    bgfx::Attachment attachments[2];
    attachments[0].init(hdrTextures[0]);
    attachments[1].init(hdrTextures[1]);
    hdrFb_.reset(bgfx::createFrameBuffer(2, attachments, true));

    // Bloom chain: progressively halved RGBA16F (no depth needed)
    for (uint16_t i = 0; i < K_BLUR_PASSES; ++i) {
        uint16_t w = width >> (i + 1);
        uint16_t h = height >> (i + 1);
        if (w < 1)
            w = 1;
        if (h < 1)
            h = 1;

        bloomFb_[i].reset(bgfx::createFrameBuffer(w, h, bgfx::TextureFormat::RGBA16F,
                                                  BGFX_TEXTURE_RT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP));
    }
}

void PostProcess::destroyFramebuffers() {
    for (auto& fb : bloomFb_) {
        fb.reset();
    }
    hdrFb_.reset();
}

} // namespace fabric
