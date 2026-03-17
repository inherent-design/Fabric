#include "fabric/render/SkyRenderer.hh"

#include "fabric/log/Log.hh"
#include "fabric/render/Rendering.hh"
#include "fabric/utils/Profiler.hh"

#include "fabric/render/FullscreenQuad.hh"
#include "fabric/render/ShaderProgram.hh"
#include "fabric/render/SpvOnly.hh"

// Compiled SPIR-V shader bytecode generated at build time from .sc sources.
#include "spv/fs_sky.sc.bin.h"
#include "spv/vs_sky.sc.bin.h"

static const bgfx::EmbeddedShader s_skyShaders[] = {BGFX_EMBEDDED_SHADER(vs_sky), BGFX_EMBEDDED_SHADER(fs_sky),
                                                    BGFX_EMBEDDED_SHADER_END()};

namespace fabric {

SkyRenderer::SkyRenderer() : sunDir_(0.0f, 0.7071f, 0.7071f) {}

SkyRenderer::~SkyRenderer() {
    shutdown();
}

void SkyRenderer::shutdown() {
    uniformParams_.reset();
    uniformSunDir_.reset();
    program_.reset();
    initialized_ = false;
}

void SkyRenderer::initProgram() {
    program_.reset(render::createProgramFromEmbedded(s_skyShaders, "vs_sky", "fs_sky"));

    uniformSunDir_.reset(bgfx::createUniform("u_sunDirection", bgfx::UniformType::Vec4));
    uniformParams_.reset(bgfx::createUniform("u_skyParams", bgfx::UniformType::Vec4));

    if (!program_.isValid() || !uniformSunDir_.isValid() || !uniformParams_.isValid()) {
        FABRIC_LOG_ERROR("SkyRenderer shader/uniform init failed for renderer {}",
                         bgfx::getRendererName(bgfx::getRendererType()));
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
    bgfx::setUniform(uniformSunDir_.get(), sunDir);

    // Upload sky parameters (turbidity in x, remaining reserved)
    float params[4] = {turbidity_, 0.0f, 0.0f, 0.0f};
    bgfx::setUniform(uniformParams_.get(), params);

    bgfx::setVertexBuffer(0, render::fullscreenTriangleVB());

    // Color-only state: no depth write, no depth test, no culling.
    // The fullscreen triangle sits at the far plane; geometry with depth
    // test enabled will naturally occlude it.
    uint64_t state = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A;
    bgfx::setState(state);

    bgfx::submit(view, program_.get());
}

void SkyRenderer::setSunDirection(const Vector3<float, Space::World>& dir) {
    sunDir_ = dir;
}

Vector3<float, Space::World> SkyRenderer::sunDirection() const {
    return sunDir_;
}

bool SkyRenderer::isValid() const {
    return program_.isValid();
}

} // namespace fabric
