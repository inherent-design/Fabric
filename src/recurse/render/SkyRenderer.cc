#include "recurse/render/SkyRenderer.hh"

#include "fabric/core/Log.hh"
#include "fabric/core/Rendering.hh"
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
#include "spv/fs_sky.sc.bin.h"
#include "spv/vs_sky.sc.bin.h"

static const bgfx::EmbeddedShader s_skyShaders[] = {BGFX_EMBEDDED_SHADER(vs_sky), BGFX_EMBEDDED_SHADER(fs_sky),
                                                    BGFX_EMBEDDED_SHADER_END()};

// Fullscreen triangle vertices in clip space.
// Three vertices that form a single triangle covering the entire viewport.
// (-1,-1) is bottom-left, (3,-1) extends past right, (-1,3) extends past top.
static const float s_skyVertices[] = {
    -1.0f, -1.0f, 0.0f, 3.0f, -1.0f, 0.0f, -1.0f, 3.0f, 0.0f,
};


using namespace fabric;

namespace recurse {

SkyRenderer::SkyRenderer()
    : program_(BGFX_INVALID_HANDLE),
      vbh_(BGFX_INVALID_HANDLE),
      uniformSunDir_(BGFX_INVALID_HANDLE),
      uniformParams_(BGFX_INVALID_HANDLE),
      sunDir_(0.0f, 0.7071f, 0.7071f) {}

SkyRenderer::~SkyRenderer() {
    shutdown();
}

void SkyRenderer::shutdown() {
    if (bgfx::isValid(uniformParams_)) {
        bgfx::destroy(uniformParams_);
    }
    if (bgfx::isValid(uniformSunDir_)) {
        bgfx::destroy(uniformSunDir_);
    }
    if (bgfx::isValid(vbh_)) {
        bgfx::destroy(vbh_);
    }
    if (bgfx::isValid(program_)) {
        bgfx::destroy(program_);
    }

    uniformParams_ = BGFX_INVALID_HANDLE;
    uniformSunDir_ = BGFX_INVALID_HANDLE;
    vbh_ = BGFX_INVALID_HANDLE;
    program_ = BGFX_INVALID_HANDLE;
    initialized_ = false;
}

void SkyRenderer::initProgram() {
    bgfx::RendererType::Enum type = bgfx::getRendererType();
    program_ = bgfx::createProgram(bgfx::createEmbeddedShader(s_skyShaders, type, "vs_sky"),
                                   bgfx::createEmbeddedShader(s_skyShaders, type, "fs_sky"), true);

    uniformSunDir_ = bgfx::createUniform("u_sunDirection", bgfx::UniformType::Vec4);
    uniformParams_ = bgfx::createUniform("u_skyParams", bgfx::UniformType::Vec4);

    // Fullscreen triangle vertex buffer
    bgfx::VertexLayout layout;
    layout.begin().add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float).end();
    vbh_ = bgfx::createVertexBuffer(bgfx::makeRef(s_skyVertices, sizeof(s_skyVertices)), layout);

    if (!bgfx::isValid(program_) || !bgfx::isValid(uniformSunDir_) || !bgfx::isValid(uniformParams_) ||
        !bgfx::isValid(vbh_)) {
        FABRIC_LOG_ERROR("SkyRenderer shader/uniform init failed for renderer {}", bgfx::getRendererName(type));
        shutdown();
        return;
    }

    initialized_ = true;

    const auto& caps = renderCaps();
    FABRIC_LOG_INFO("SkyRenderer initialized: backend={}, tier={}", caps.rendererName, static_cast<int>(caps.tier));
}

void SkyRenderer::init() {
    if (!initialized_) {
        initProgram();
    }
}

void SkyRenderer::render(bgfx::ViewId view) {
    FABRIC_ZONE_SCOPED;

    if (!isValid()) {
        return;
    }

    // Upload sun direction
    float sunDir[4] = {sunDir_.x, sunDir_.y, sunDir_.z, 0.0f};
    bgfx::setUniform(uniformSunDir_, sunDir);

    // Upload sky parameters (turbidity in x, remaining reserved)
    float params[4] = {turbidity_, 0.0f, 0.0f, 0.0f};
    bgfx::setUniform(uniformParams_, params);

    bgfx::setVertexBuffer(0, vbh_);

    // Color-only state: no depth write, no depth test, no culling.
    // The fullscreen triangle sits at the far plane; geometry with depth
    // test enabled will naturally occlude it.
    uint64_t state = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A;
    bgfx::setState(state);

    bgfx::submit(view, program_);
}

void SkyRenderer::setSunDirection(const Vector3<float, Space::World>& dir) {
    sunDir_ = dir;
}

Vector3<float, Space::World> SkyRenderer::sunDirection() const {
    return sunDir_;
}

bool SkyRenderer::isValid() const {
    return bgfx::isValid(program_);
}

} // namespace recurse
