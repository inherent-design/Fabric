#include "fabric/core/PostProcess.hh"

#include "fabric/core/Log.hh"
#include "fabric/core/Rendering.hh"
#include "fabric/utils/Profiler.hh"

// Suppress shader profiles we don't compile per-platform.
#if !defined(_WIN32)
#define BGFX_PLATFORM_SUPPORTS_DXBC 0
#endif
#define BGFX_PLATFORM_SUPPORTS_DXIL 0
#define BGFX_PLATFORM_SUPPORTS_WGSL 0
#include <bgfx/embedded_shader.h>

// Compiled shader bytecode generated at build time from .sc sources.
#include "essl/fs_blur.sc.bin.h"
#include "essl/fs_bright.sc.bin.h"
#include "essl/fs_tonemap.sc.bin.h"
#include "essl/vs_fullscreen.sc.bin.h"
#include "glsl/fs_blur.sc.bin.h"
#include "glsl/fs_bright.sc.bin.h"
#include "glsl/fs_tonemap.sc.bin.h"
#include "glsl/vs_fullscreen.sc.bin.h"
#include "spv/fs_blur.sc.bin.h"
#include "spv/fs_bright.sc.bin.h"
#include "spv/fs_tonemap.sc.bin.h"
#include "spv/vs_fullscreen.sc.bin.h"
#if BX_PLATFORM_WINDOWS
#include "dxbc/fs_blur.sc.bin.h"
#include "dxbc/fs_bright.sc.bin.h"
#include "dxbc/fs_tonemap.sc.bin.h"
#include "dxbc/vs_fullscreen.sc.bin.h"
#endif
#if BX_PLATFORM_OSX || BX_PLATFORM_IOS || BX_PLATFORM_VISIONOS
#include "mtl/fs_blur.sc.bin.h"
#include "mtl/fs_bright.sc.bin.h"
#include "mtl/fs_tonemap.sc.bin.h"
#include "mtl/vs_fullscreen.sc.bin.h"
#endif

static const bgfx::EmbeddedShader s_postShaders[] = {BGFX_EMBEDDED_SHADER(vs_fullscreen),
                                                     BGFX_EMBEDDED_SHADER(fs_bright), BGFX_EMBEDDED_SHADER(fs_blur),
                                                     BGFX_EMBEDDED_SHADER(fs_tonemap), BGFX_EMBEDDED_SHADER_END()};

// Fullscreen triangle vertices in clip space.
static const float s_fullscreenVertices[] = {
    -1.0f, -1.0f, 0.0f, 3.0f, -1.0f, 0.0f, -1.0f, 3.0f, 0.0f,
};

namespace fabric {

PostProcess::PostProcess()
    : brightProgram_(BGFX_INVALID_HANDLE),
      blurProgram_(BGFX_INVALID_HANDLE),
      tonemapProgram_(BGFX_INVALID_HANDLE),
      vbh_(BGFX_INVALID_HANDLE),
      uniformBloomParams_(BGFX_INVALID_HANDLE),
      uniformTexelSize_(BGFX_INVALID_HANDLE),
      uniformTonemapParams_(BGFX_INVALID_HANDLE),
      samplerHdrColor_(BGFX_INVALID_HANDLE),
      samplerBloomTex_(BGFX_INVALID_HANDLE),
      samplerInputTex_(BGFX_INVALID_HANDLE),
      hdrFb_(BGFX_INVALID_HANDLE) {
    for (auto& fb : bloomFb_) {
        fb = BGFX_INVALID_HANDLE;
    }
}

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
    if (!bgfx::isValid(brightProgram_)) {
        return;
    }

    createFramebuffers(width, height);
    initialized_ = true;

    const auto& caps = renderCaps();
    FABRIC_LOG_INFO("PostProcess initialized: {}x{}, backend={}", width, height, caps.rendererName);
}

void PostProcess::shutdown() {
    destroyFramebuffers();

    if (bgfx::isValid(samplerInputTex_)) {
        bgfx::destroy(samplerInputTex_);
    }
    if (bgfx::isValid(samplerBloomTex_)) {
        bgfx::destroy(samplerBloomTex_);
    }
    if (bgfx::isValid(samplerHdrColor_)) {
        bgfx::destroy(samplerHdrColor_);
    }
    if (bgfx::isValid(uniformTonemapParams_)) {
        bgfx::destroy(uniformTonemapParams_);
    }
    if (bgfx::isValid(uniformTexelSize_)) {
        bgfx::destroy(uniformTexelSize_);
    }
    if (bgfx::isValid(uniformBloomParams_)) {
        bgfx::destroy(uniformBloomParams_);
    }
    if (bgfx::isValid(vbh_)) {
        bgfx::destroy(vbh_);
    }
    if (bgfx::isValid(tonemapProgram_)) {
        bgfx::destroy(tonemapProgram_);
    }
    if (bgfx::isValid(blurProgram_)) {
        bgfx::destroy(blurProgram_);
    }
    if (bgfx::isValid(brightProgram_)) {
        bgfx::destroy(brightProgram_);
    }

    brightProgram_ = BGFX_INVALID_HANDLE;
    blurProgram_ = BGFX_INVALID_HANDLE;
    tonemapProgram_ = BGFX_INVALID_HANDLE;
    vbh_ = BGFX_INVALID_HANDLE;
    uniformBloomParams_ = BGFX_INVALID_HANDLE;
    uniformTexelSize_ = BGFX_INVALID_HANDLE;
    uniformTonemapParams_ = BGFX_INVALID_HANDLE;
    samplerHdrColor_ = BGFX_INVALID_HANDLE;
    samplerBloomTex_ = BGFX_INVALID_HANDLE;
    samplerInputTex_ = BGFX_INVALID_HANDLE;
    hdrFb_ = BGFX_INVALID_HANDLE;
    for (auto& fb : bloomFb_) {
        fb = BGFX_INVALID_HANDLE;
    }

    width_ = 0;
    height_ = 0;
    initialized_ = false;
}

bool PostProcess::isValid() const {
    return initialized_ && bgfx::isValid(brightProgram_);
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
    return hdrFb_;
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
        bgfx::setViewFrameBuffer(view, bloomFb_[0]);
        bgfx::setViewClear(view, BGFX_CLEAR_COLOR, 0x000000ff, 1.0f, 0);

        float bloomParams[4] = {threshold_, threshold_ * 0.5f, 0.0f, 0.0f};
        bgfx::setUniform(uniformBloomParams_, bloomParams);

        bgfx::setTexture(0, samplerHdrColor_, bgfx::getTexture(hdrFb_, 0));
        bgfx::setVertexBuffer(0, vbh_);
        bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
        bgfx::submit(view, brightProgram_);
    }

    // Passes 1..kBlurPasses-1: Dual Kawase blur (downsample chain)
    for (uint16_t i = 1; i < kBlurPasses; ++i) {
        uint8_t view = baseViewId + i;
        bgfx::setViewName(view, "Bloom Blur");

        uint16_t w = width_ >> (i + 1);
        uint16_t h = height_ >> (i + 1);
        if (w < 1)
            w = 1;
        if (h < 1)
            h = 1;

        bgfx::setViewRect(view, 0, 0, w, h);
        bgfx::setViewFrameBuffer(view, bloomFb_[i]);
        bgfx::setViewClear(view, BGFX_CLEAR_COLOR, 0x000000ff, 1.0f, 0);

        float texelSize[4] = {1.0f / static_cast<float>(w), 1.0f / static_cast<float>(h), 0.0f, static_cast<float>(i)};
        bgfx::setUniform(uniformTexelSize_, texelSize);

        bgfx::setTexture(0, samplerInputTex_, bgfx::getTexture(bloomFb_[i - 1], 0));
        bgfx::setVertexBuffer(0, vbh_);
        bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
        bgfx::submit(view, blurProgram_);
    }

    // Final pass: Tonemapping composite to backbuffer
    {
        uint8_t view = baseViewId + kBlurPasses;
        bgfx::setViewName(view, "Tonemap");
        bgfx::setViewRect(view, 0, 0, width_, height_);
        bgfx::setViewFrameBuffer(view, BGFX_INVALID_HANDLE); // backbuffer
        bgfx::setViewClear(view, BGFX_CLEAR_NONE);

        float tonemapParams[4] = {intensity_, exposure_, 0.0f, 0.0f};
        bgfx::setUniform(uniformTonemapParams_, tonemapParams);

        bgfx::setTexture(0, samplerHdrColor_, bgfx::getTexture(hdrFb_, 0));
        bgfx::setTexture(1, samplerBloomTex_, bgfx::getTexture(bloomFb_[kBlurPasses - 1], 0));
        bgfx::setVertexBuffer(0, vbh_);
        bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
        bgfx::submit(view, tonemapProgram_);
    }
}

void PostProcess::initPrograms() {
    bgfx::RendererType::Enum type = bgfx::getRendererType();

    auto vs = bgfx::createEmbeddedShader(s_postShaders, type, "vs_fullscreen");

    brightProgram_ = bgfx::createProgram(vs, bgfx::createEmbeddedShader(s_postShaders, type, "fs_bright"), false);
    blurProgram_ = bgfx::createProgram(bgfx::createEmbeddedShader(s_postShaders, type, "vs_fullscreen"),
                                       bgfx::createEmbeddedShader(s_postShaders, type, "fs_blur"), true);
    tonemapProgram_ = bgfx::createProgram(bgfx::createEmbeddedShader(s_postShaders, type, "vs_fullscreen"),
                                          bgfx::createEmbeddedShader(s_postShaders, type, "fs_tonemap"), true);

    // The first program shares vs; destroy vs separately only if bright fails
    if (!bgfx::isValid(brightProgram_)) {
        bgfx::destroy(vs);
    }

    uniformBloomParams_ = bgfx::createUniform("u_bloomParams", bgfx::UniformType::Vec4);
    uniformTexelSize_ = bgfx::createUniform("u_texelSize", bgfx::UniformType::Vec4);
    uniformTonemapParams_ = bgfx::createUniform("u_tonemapParams", bgfx::UniformType::Vec4);
    samplerHdrColor_ = bgfx::createUniform("s_hdrColor", bgfx::UniformType::Sampler);
    samplerBloomTex_ = bgfx::createUniform("s_bloomTex", bgfx::UniformType::Sampler);
    samplerInputTex_ = bgfx::createUniform("s_inputTex", bgfx::UniformType::Sampler);

    // Fullscreen triangle vertex buffer
    bgfx::VertexLayout layout;
    layout.begin().add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float).end();
    vbh_ = bgfx::createVertexBuffer(bgfx::makeRef(s_fullscreenVertices, sizeof(s_fullscreenVertices)), layout);

    if (!bgfx::isValid(brightProgram_) || !bgfx::isValid(blurProgram_) || !bgfx::isValid(tonemapProgram_) ||
        !bgfx::isValid(vbh_)) {
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
    hdrFb_ = bgfx::createFrameBuffer(2, attachments, true);

    // Bloom chain: progressively halved RGBA16F (no depth needed)
    for (uint16_t i = 0; i < kBlurPasses; ++i) {
        uint16_t w = width >> (i + 1);
        uint16_t h = height >> (i + 1);
        if (w < 1)
            w = 1;
        if (h < 1)
            h = 1;

        bloomFb_[i] = bgfx::createFrameBuffer(w, h, bgfx::TextureFormat::RGBA16F,
                                              BGFX_TEXTURE_RT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP);
    }
}

void PostProcess::destroyFramebuffers() {
    for (auto& fb : bloomFb_) {
        if (bgfx::isValid(fb)) {
            bgfx::destroy(fb);
            fb = BGFX_INVALID_HANDLE;
        }
    }
    if (bgfx::isValid(hdrFb_)) {
        bgfx::destroy(hdrFb_);
        hdrFb_ = BGFX_INVALID_HANDLE;
    }
}

} // namespace fabric
