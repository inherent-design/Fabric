#include "fabric/render/PaniniPass.hh"

#include "fabric/log/Log.hh"
#include "fabric/utils/Profiler.hh"
#include <algorithm> // for std::lerp
#include <cmath>

#include "fabric/render/FullscreenQuad.hh"
#include "fabric/render/ShaderProgram.hh"
#include "fabric/render/SpvOnly.hh"

// Compiled SPIR-V shader bytecode generated at build time from .sc sources.
#include "spv/fs_panini.sc.bin.h"
#include "spv/vs_fullscreen.sc.bin.h"

static const bgfx::EmbeddedShader s_paniniShaders[] = {BGFX_EMBEDDED_SHADER(vs_fullscreen),
                                                       BGFX_EMBEDDED_SHADER(fs_panini), BGFX_EMBEDDED_SHADER_END()};

namespace fabric {

PaniniPass::PaniniPass() = default;

PaniniPass::~PaniniPass() {
    shutdown();
}

void PaniniPass::init(uint16_t width, uint16_t height) {
    if (initialized_) {
        return;
    }

    if (width == 0 || height == 0) {
        FABRIC_LOG_WARN("PaniniPass::init called with zero dimensions ({}x{})", width, height);
        return;
    }

    width_ = width;
    height_ = height;

    initPrograms();

    if (!program_.isValid()) {
        FABRIC_LOG_ERROR("PaniniPass shader init failed");
        shutdown();
        return;
    }

    initialized_ = true;
    FABRIC_LOG_INFO("PaniniPass initialized: {}x{}, enabled={}", width, height, enabled_);
}

void PaniniPass::shutdown() {
    u_paniniExtra_.reset();
    u_viewportSize_.reset();
    u_params_.reset();
    u_sceneTex_.reset();
    program_.reset();

    width_ = 0;
    height_ = 0;
    initialized_ = false;
}

void PaniniPass::resize(uint16_t width, uint16_t height) {
    if (width == 0 || height == 0) {
        return;
    }
    width_ = width;
    height_ = height;
}

bool PaniniPass::isValid() const {
    return initialized_ && program_.isValid();
}

void PaniniPass::execute(bgfx::TextureHandle sceneColor, bgfx::ViewId viewId) {
    FABRIC_ZONE_SCOPED_N("PaniniPass::execute");

    if (!initialized_ || !program_.isValid()) {
        return;
    }

    // When disabled or strength is effectively zero, skip projection
    if (!enabled_ || strength_ < 0.001f) {
        // Still need to pass through - this is handled by shader for simplicity
        // The shader will check enabled flag and do passthrough
    }

    bgfx::setViewName(viewId, "Panini Projection");
    bgfx::setViewRect(viewId, 0, 0, width_, height_);
    // Output goes to whatever framebuffer is set by caller (or backbuffer if none)

    // Compute half_tan_fov from FOV degrees
    float halfFovRad = (fovDeg_ * 0.5f) * (3.14159265359f / 180.0f);
    float halfTanFov = std::tan(halfFovRad);

    // Compute fill zoom if fill is enabled
    float zoom = 1.0f;
    if (fill_ > 0.001f && strength_ > 0.001f) {
        float aspect = static_cast<float>(width_) / static_cast<float>(height_);
        float lonMax = std::atan(halfTanFov * aspect);
        float denom = strength_ + std::cos(lonMax);
        if (denom > 0.001f) {
            float k = (strength_ + 1.0f) / denom;
            float edgeX = k * std::sin(lonMax);
            float fitX = std::abs(edgeX) / (halfTanFov * aspect);
            if (fitX > 0.001f) {
                zoom = 1.0f / fitX;
            }
        }
        zoom = std::lerp(1.0f, zoom, fill_);
    }

    float aspect = static_cast<float>(width_) / static_cast<float>(height_);

    // Set uniforms
    float params[4] = {strength_, halfTanFov, zoom, enabled_ ? 1.0f : 0.0f};
    bgfx::setUniform(u_params_.get(), params);

    float viewportSize[4] = {static_cast<float>(width_), static_cast<float>(height_), 1.0f / static_cast<float>(width_),
                             1.0f / static_cast<float>(height_)};
    bgfx::setUniform(u_viewportSize_.get(), viewportSize);

    float paniniExtra[4] = {verticalComp_, aspect, 0.0f, 0.0f};
    bgfx::setUniform(u_paniniExtra_.get(), paniniExtra);

    // Bind scene texture
    bgfx::setTexture(0, u_sceneTex_.get(), sceneColor);

    // Set vertex buffer and state
    bgfx::setVertexBuffer(0, render::fullscreenTriangleVB());
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);

    // Submit
    bgfx::submit(viewId, program_.get());
}

void PaniniPass::setEnabled(bool enabled) {
    enabled_ = enabled;
    FABRIC_LOG_DEBUG("PaniniPass enabled: {}", enabled_);
}

void PaniniPass::setStrength(float d) {
    strength_ = std::clamp(d, 0.0f, 1.0f);
}

void PaniniPass::setFOV(float degrees) {
    fovDeg_ = std::clamp(degrees, 60.0f, 180.0f);
}

void PaniniPass::setFill(float fill) {
    fill_ = std::clamp(fill, 0.0f, 1.0f);
}

void PaniniPass::setVerticalCompensation(float s) {
    verticalComp_ = std::clamp(s, -1.0f, 1.0f);
}

bool PaniniPass::isEnabled() const {
    return enabled_;
}

float PaniniPass::fovDeg() const {
    return fovDeg_;
}

void PaniniPass::cycleEnabled() {
    enabled_ = !enabled_;
    FABRIC_LOG_INFO("PaniniPass toggled: {}", enabled_ ? "ON" : "OFF");
}

void PaniniPass::cycleStrength() {
    // Cycle through presets: 0.3 -> 0.5 -> 0.7 -> 1.0 -> 0.3
    constexpr float strengths[] = {0.3f, 0.5f, 0.7f, 1.0f};
    constexpr int numStrengths = sizeof(strengths) / sizeof(strengths[0]);

    int currentIdx = 0;
    for (int i = 0; i < numStrengths; ++i) {
        if (std::abs(strength_ - strengths[i]) < 0.05f) {
            currentIdx = i;
            break;
        }
    }

    strength_ = strengths[(currentIdx + 1) % numStrengths];
    FABRIC_LOG_INFO("PaniniPass strength: {}", strength_);
}

void PaniniPass::initPrograms() {
    program_.reset(render::createProgramFromEmbedded(s_paniniShaders, "vs_fullscreen", "fs_panini"));

    u_sceneTex_.reset(bgfx::createUniform("s_sceneTex", bgfx::UniformType::Sampler));
    u_params_.reset(bgfx::createUniform("u_params", bgfx::UniformType::Vec4));
    u_viewportSize_.reset(bgfx::createUniform("u_viewportSize", bgfx::UniformType::Vec4));
    u_paniniExtra_.reset(bgfx::createUniform("u_paniniExtra", bgfx::UniformType::Vec4));

    if (!program_.isValid()) {
        FABRIC_LOG_ERROR("PaniniPass failed to create shader program for renderer {}",
                         bgfx::getRendererName(bgfx::getRendererType()));
    }
}

} // namespace fabric
