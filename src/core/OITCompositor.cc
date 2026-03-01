#include "fabric/core/OITCompositor.hh"

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
#include "essl/fs_oit_accum.sc.bin.h"
#include "essl/fs_oit_composite.sc.bin.h"
#include "essl/vs_oit_accum.sc.bin.h"
#include "essl/vs_oit_composite.sc.bin.h"
#include "glsl/fs_oit_accum.sc.bin.h"
#include "glsl/fs_oit_composite.sc.bin.h"
#include "glsl/vs_oit_accum.sc.bin.h"
#include "glsl/vs_oit_composite.sc.bin.h"
#include "spv/fs_oit_accum.sc.bin.h"
#include "spv/fs_oit_composite.sc.bin.h"
#include "spv/vs_oit_accum.sc.bin.h"
#include "spv/vs_oit_composite.sc.bin.h"
#if BX_PLATFORM_WINDOWS
#include "dxbc/fs_oit_accum.sc.bin.h"
#include "dxbc/fs_oit_composite.sc.bin.h"
#include "dxbc/vs_oit_accum.sc.bin.h"
#include "dxbc/vs_oit_composite.sc.bin.h"
#endif
#if BX_PLATFORM_OSX || BX_PLATFORM_IOS || BX_PLATFORM_VISIONOS
#include "mtl/fs_oit_accum.sc.bin.h"
#include "mtl/fs_oit_composite.sc.bin.h"
#include "mtl/vs_oit_accum.sc.bin.h"
#include "mtl/vs_oit_composite.sc.bin.h"
#endif

static const bgfx::EmbeddedShader s_oitShaders[] = {
    BGFX_EMBEDDED_SHADER(vs_oit_accum), BGFX_EMBEDDED_SHADER(fs_oit_accum), BGFX_EMBEDDED_SHADER(vs_oit_composite),
    BGFX_EMBEDDED_SHADER(fs_oit_composite), BGFX_EMBEDDED_SHADER_END()};

// Fullscreen triangle vertices in clip space (same as PostProcess).
static const float s_fullscreenVertices[] = {
    -1.0f, -1.0f, 0.0f, 3.0f, -1.0f, 0.0f, -1.0f, 3.0f, 0.0f,
};

namespace fabric {

OITCompositor::OITCompositor()
    : accumProgram_(BGFX_INVALID_HANDLE),
      compositeProgram_(BGFX_INVALID_HANDLE),
      vbh_(BGFX_INVALID_HANDLE),
      uniformOitColor_(BGFX_INVALID_HANDLE),
      samplerAccum_(BGFX_INVALID_HANDLE),
      samplerRevealage_(BGFX_INVALID_HANDLE),
      oitFb_(BGFX_INVALID_HANDLE) {}

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
    if (!bgfx::isValid(accumProgram_) || !bgfx::isValid(compositeProgram_)) {
        FABRIC_LOG_ERROR("OITCompositor: shader init failed");
        shutdown();
        return false;
    }

    createFramebuffers(width, height);
    if (!bgfx::isValid(oitFb_)) {
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

    if (bgfx::isValid(samplerRevealage_)) {
        bgfx::destroy(samplerRevealage_);
    }
    if (bgfx::isValid(samplerAccum_)) {
        bgfx::destroy(samplerAccum_);
    }
    if (bgfx::isValid(uniformOitColor_)) {
        bgfx::destroy(uniformOitColor_);
    }
    if (bgfx::isValid(vbh_)) {
        bgfx::destroy(vbh_);
    }
    if (bgfx::isValid(compositeProgram_)) {
        bgfx::destroy(compositeProgram_);
    }
    if (bgfx::isValid(accumProgram_)) {
        bgfx::destroy(accumProgram_);
    }

    accumProgram_ = BGFX_INVALID_HANDLE;
    compositeProgram_ = BGFX_INVALID_HANDLE;
    vbh_ = BGFX_INVALID_HANDLE;
    uniformOitColor_ = BGFX_INVALID_HANDLE;
    samplerAccum_ = BGFX_INVALID_HANDLE;
    samplerRevealage_ = BGFX_INVALID_HANDLE;
    oitFb_ = BGFX_INVALID_HANDLE;

    width_ = 0;
    height_ = 0;
    initialized_ = false;
}

bool OITCompositor::isValid() const {
    return initialized_ && bgfx::isValid(accumProgram_) && bgfx::isValid(compositeProgram_) && bgfx::isValid(oitFb_);
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
    bgfx::setViewFrameBuffer(viewId, oitFb_);
    bgfx::setViewTransform(viewId, viewMtx, projMtx);

    // Clear accumulation to (0,0,0,0) and revealage to (1,0,0,1)
    // Using BGFX_CLEAR_COLOR clears all attachments; we set appropriate initial values.
    // Accum target: cleared to 0. Revealage target: cleared to 1 (fully revealed = no transparency).
    // bgfx clears all RT attachments with the same color, so we clear to (0,0,0,0)
    // and the revealage shader writes initial 1.0 via blend.
    bgfx::setViewClear(viewId, BGFX_CLEAR_COLOR, 0x00000000, 1.0f, 0);
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
    bgfx::setTexture(0, samplerAccum_, bgfx::getTexture(oitFb_, 0));
    bgfx::setTexture(1, samplerRevealage_, bgfx::getTexture(oitFb_, 1));

    bgfx::setVertexBuffer(0, vbh_);

    // Alpha blending: srcAlpha * src + (1 - srcAlpha) * dst
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A |
                   BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_INV_SRC_ALPHA));
    bgfx::submit(viewId, compositeProgram_);
}

void OITCompositor::setColor(float r, float g, float b, float a) {
    color_[0] = r;
    color_[1] = g;
    color_[2] = b;
    color_[3] = a;
}

bgfx::ProgramHandle OITCompositor::accumProgram() const {
    return accumProgram_;
}

bgfx::FrameBufferHandle OITCompositor::framebuffer() const {
    return oitFb_;
}

bgfx::UniformHandle OITCompositor::colorUniform() const {
    return uniformOitColor_;
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
    accumProgram_ = bgfx::createProgram(accumVs, accumFs, true);

    auto compositeVs = bgfx::createEmbeddedShader(s_oitShaders, type, "vs_oit_composite");
    auto compositeFs = bgfx::createEmbeddedShader(s_oitShaders, type, "fs_oit_composite");
    compositeProgram_ = bgfx::createProgram(compositeVs, compositeFs, true);

    uniformOitColor_ = bgfx::createUniform("u_oitColor", bgfx::UniformType::Vec4);
    samplerAccum_ = bgfx::createUniform("s_oitAccum", bgfx::UniformType::Sampler);
    samplerRevealage_ = bgfx::createUniform("s_oitRevealage", bgfx::UniformType::Sampler);

    // Fullscreen triangle vertex buffer (for composite pass)
    bgfx::VertexLayout layout;
    layout.begin().add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float).end();
    vbh_ = bgfx::createVertexBuffer(bgfx::makeRef(s_fullscreenVertices, sizeof(s_fullscreenVertices)), layout);

    if (!bgfx::isValid(accumProgram_) || !bgfx::isValid(compositeProgram_) || !bgfx::isValid(vbh_)) {
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
    oitFb_ = bgfx::createFrameBuffer(2, attachments, true);
}

void OITCompositor::destroyFramebuffers() {
    if (bgfx::isValid(oitFb_)) {
        bgfx::destroy(oitFb_);
        oitFb_ = BGFX_INVALID_HANDLE;
    }
}

} // namespace fabric
