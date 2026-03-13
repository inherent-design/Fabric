#include "recurse/render/OITCompositor.hh"

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
#include "spv/fs_oit_accum.sc.bin.h"
#include "spv/fs_oit_composite.sc.bin.h"
#include "spv/vs_fullscreen.sc.bin.h"
#include "spv/vs_oit_accum.sc.bin.h"

static const bgfx::EmbeddedShader s_oitShaders[] = {
    BGFX_EMBEDDED_SHADER(vs_oit_accum), BGFX_EMBEDDED_SHADER(fs_oit_accum), BGFX_EMBEDDED_SHADER(vs_fullscreen),
    BGFX_EMBEDDED_SHADER(fs_oit_composite), BGFX_EMBEDDED_SHADER_END()};

// Fullscreen triangle vertices in clip space (same as PostProcess).
static const float s_fullscreenVertices[] = {
    -1.0f, -1.0f, 0.0f, 3.0f, -1.0f, 0.0f, -1.0f, 3.0f, 0.0f,
};

using namespace fabric;

namespace recurse {

OITCompositor::OITCompositor() = default;

OITCompositor::~OITCompositor() {
    shutdown();
}

bool OITCompositor::init(uint16_t width, uint16_t height) {
    FABRIC_ZONE_SCOPED_N("OITCompositor::init");

    if (initialized_) {
        return true;
    }

    if (width == 0 || height == 0) {
        FABRIC_LOG_WARN("OITCompositor::init called with zero dimensions ({}x{})", width, height);
        return false;
    }

    // Check hardware requirements: MRT and RGBA16F
    const auto& caps = renderCaps();
    if (!caps.mrt) {
        FABRIC_LOG_WARN("OITCompositor: MRT not supported, OIT disabled");
        return false;
    }

    // Check RGBA16F format support via bgfx::getCaps()
    const bgfx::Caps* bgfxCaps = bgfx::getCaps();
    if (bgfxCaps) {
        bool hasRGBA16F = (bgfxCaps->formats[bgfx::TextureFormat::RGBA16F] & BGFX_CAPS_FORMAT_TEXTURE_FRAMEBUFFER) != 0;
        if (!hasRGBA16F) {
            FABRIC_LOG_WARN("OITCompositor: RGBA16F framebuffer not supported, OIT disabled");
            return false;
        }
    }

    width_ = width;
    height_ = height;

    initPrograms();
    if (!accumProgram_.isValid() || !compositeProgram_.isValid()) {
        FABRIC_LOG_ERROR("OITCompositor: shader init failed");
        shutdown();
        return false;
    }

    createFramebuffers(width, height);
    if (!oitFb_.isValid()) {
        FABRIC_LOG_ERROR("OITCompositor: framebuffer creation failed");
        shutdown();
        return false;
    }

    initialized_ = true;
    FABRIC_LOG_INFO("OITCompositor initialized: {}x{}, backend={}", width, height, caps.rendererName);
    return true;
}

void OITCompositor::shutdown() {
    destroyFramebuffers();

    samplerRevealage_.reset();
    samplerAccum_.reset();
    uniformOitColor_.reset();
    vbh_.reset();
    compositeProgram_.reset();
    accumProgram_.reset();

    width_ = 0;
    height_ = 0;
    initialized_ = false;
}

bool OITCompositor::isValid() const {
    return initialized_ && accumProgram_.isValid() && compositeProgram_.isValid() && oitFb_.isValid();
}

void OITCompositor::beginAccumulation(uint8_t viewId, const float* viewMtx, const float* projMtx, uint16_t width,
                                      uint16_t height) {
    FABRIC_ZONE_SCOPED_N("OITCompositor::beginAccumulation");

    if (!isValid()) {
        return;
    }

    accumViewId_ = viewId;

    bgfx::setViewName(viewId, "OIT Accumulation");
    bgfx::setViewRect(viewId, 0, 0, width, height);
    bgfx::setViewFrameBuffer(viewId, oitFb_.get());
    bgfx::setViewTransform(viewId, viewMtx, projMtx);

    // Per-attachment clear via bgfx palette colors.
    // Accumulation (attachment 0): must start at (0,0,0,0) for additive blending.
    // Revealage (attachment 1): must start at R=1.0 (fully revealed = no transparency).
    // bgfx::setViewClear with palette indices allows different clear values per MRT attachment.
    const float clearAccum[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    const float clearReveal[4] = {1.0f, 0.0f, 0.0f, 0.0f};
    bgfx::setPaletteColor(0, clearAccum);
    bgfx::setPaletteColor(1, clearReveal);
    bgfx::setViewClear(viewId, BGFX_CLEAR_COLOR, 1.0f, 0, 0, 1);
    bgfx::touch(viewId);
}

void OITCompositor::composite(uint8_t viewId, uint16_t width, uint16_t height) {
    FABRIC_ZONE_SCOPED_N("OITCompositor::composite");

    if (!isValid()) {
        return;
    }

    compositeViewId_ = viewId;

    bgfx::setViewName(viewId, "OIT Composite");
    bgfx::setViewRect(viewId, 0, 0, width, height);
    bgfx::setViewFrameBuffer(viewId, BGFX_INVALID_HANDLE); // render to backbuffer (or current target)
    bgfx::setViewClear(viewId, BGFX_CLEAR_NONE);

    // Bind OIT textures
    bgfx::setTexture(0, samplerAccum_.get(), bgfx::getTexture(oitFb_.get(), 0));
    bgfx::setTexture(1, samplerRevealage_.get(), bgfx::getTexture(oitFb_.get(), 1));

    bgfx::setVertexBuffer(0, vbh_.get());

    // Alpha blending: srcAlpha * src + (1 - srcAlpha) * dst
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A |
                   BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_INV_SRC_ALPHA));
    bgfx::submit(viewId, compositeProgram_.get());
}

void OITCompositor::setColor(float r, float g, float b, float a) {
    color_[0] = r;
    color_[1] = g;
    color_[2] = b;
    color_[3] = a;
}

bgfx::ProgramHandle OITCompositor::accumProgram() const {
    return accumProgram_.get();
}

bgfx::FrameBufferHandle OITCompositor::framebuffer() const {
    return oitFb_.get();
}

bgfx::UniformHandle OITCompositor::colorUniform() const {
    return uniformOitColor_.get();
}

uint8_t OITCompositor::accumViewId() const {
    return accumViewId_;
}

uint8_t OITCompositor::compositeViewId() const {
    return compositeViewId_;
}

void OITCompositor::initPrograms() {
    bgfx::RendererType::Enum type = bgfx::getRendererType();

    auto accumVs = bgfx::createEmbeddedShader(s_oitShaders, type, "vs_oit_accum");
    auto accumFs = bgfx::createEmbeddedShader(s_oitShaders, type, "fs_oit_accum");
    accumProgram_.reset(bgfx::createProgram(accumVs, accumFs, true));

    auto compositeVs = bgfx::createEmbeddedShader(s_oitShaders, type, "vs_fullscreen");
    auto compositeFs = bgfx::createEmbeddedShader(s_oitShaders, type, "fs_oit_composite");
    compositeProgram_.reset(bgfx::createProgram(compositeVs, compositeFs, true));

    uniformOitColor_.reset(bgfx::createUniform("u_oitColor", bgfx::UniformType::Vec4));
    samplerAccum_.reset(bgfx::createUniform("s_oitAccum", bgfx::UniformType::Sampler));
    samplerRevealage_.reset(bgfx::createUniform("s_oitRevealage", bgfx::UniformType::Sampler));

    // Fullscreen triangle vertex buffer (for composite pass)
    bgfx::VertexLayout layout;
    layout.begin().add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float).end();
    vbh_.reset(bgfx::createVertexBuffer(bgfx::makeRef(s_fullscreenVertices, sizeof(s_fullscreenVertices)), layout));

    if (!accumProgram_.isValid() || !compositeProgram_.isValid() || !vbh_.isValid()) {
        FABRIC_LOG_ERROR("OITCompositor shader init failed for renderer {}", bgfx::getRendererName(type));
        shutdown();
    }
}

void OITCompositor::createFramebuffers(uint16_t width, uint16_t height) {
    // OIT framebuffer: RGBA16F accumulation + R8 revealage
    bgfx::TextureHandle oitTextures[2];
    oitTextures[0] = bgfx::createTexture2D(width, height, false, 1, bgfx::TextureFormat::RGBA16F,
                                           BGFX_TEXTURE_RT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP);
    oitTextures[1] = bgfx::createTexture2D(width, height, false, 1, bgfx::TextureFormat::R8,
                                           BGFX_TEXTURE_RT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP);

    bgfx::Attachment attachments[2];
    attachments[0].init(oitTextures[0]);
    attachments[1].init(oitTextures[1]);
    oitFb_.reset(bgfx::createFrameBuffer(2, attachments, true));
}

void OITCompositor::destroyFramebuffers() {
    oitFb_.reset();
}

} // namespace recurse
