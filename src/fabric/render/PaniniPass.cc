#include "fabric/render/PaniniPass.hh"

#include "fabric/core/Log.hh"
#include "fabric/render/Rendering.hh"
#include "fabric/utils/Profiler.hh"
#include <algorithm> // for std::lerp
#include <cmath>

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
#include "spv/fs_panini.sc.bin.h"
#include "spv/vs_panini.sc.bin.h"

static const bgfx::EmbeddedShader s_paniniShaders[] = {BGFX_EMBEDDED_SHADER(vs_panini), BGFX_EMBEDDED_SHADER(fs_panini),
                                                       BGFX_EMBEDDED_SHADER_END()};

// Fullscreen triangle vertices in clip space.
static const float s_fullscreenVertices[] = {
    -1.0f, -1.0f, 0.0f, // vertex 0
    3.0f,  -1.0f, 0.0f, // vertex 1
    -1.0f, 3.0f,  0.0f, // vertex 2
};

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
    createVertexBuffer();

    if (!program_.isValid()) {
        FABRIC_LOG_ERROR("PaniniPass shader init failed");
        shutdown();
        return;
    }

    initialized_ = true;
    FABRIC_LOG_INFO("PaniniPass initialized: {}x{}, enabled={}", width, height, enabled_);
}

void PaniniPass::shutdown() {
    fullscreenQuad_.reset();
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
    bgfx::setVertexBuffer(0, fullscreenQuad_.get());
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
    bgfx::RendererType::Enum type = bgfx::getRendererType();

    program_.reset(bgfx::createProgram(bgfx::createEmbeddedShader(s_paniniShaders, type, "vs_panini"),
                                       bgfx::createEmbeddedShader(s_paniniShaders, type, "fs_panini"), true));

    u_sceneTex_.reset(bgfx::createUniform("s_sceneTex", bgfx::UniformType::Sampler));
    u_params_.reset(bgfx::createUniform("u_params", bgfx::UniformType::Vec4));
    u_viewportSize_.reset(bgfx::createUniform("u_viewportSize", bgfx::UniformType::Vec4));
    u_paniniExtra_.reset(bgfx::createUniform("u_paniniExtra", bgfx::UniformType::Vec4));

    if (!program_.isValid()) {
        FABRIC_LOG_ERROR("PaniniPass failed to create shader program for renderer {}", bgfx::getRendererName(type));
    }
}

void PaniniPass::createVertexBuffer() {
    bgfx::VertexLayout layout;
    layout.begin().add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float).end();
    fullscreenQuad_.reset(
        bgfx::createVertexBuffer(bgfx::makeRef(s_fullscreenVertices, sizeof(s_fullscreenVertices)), layout));
}

} // namespace fabric
