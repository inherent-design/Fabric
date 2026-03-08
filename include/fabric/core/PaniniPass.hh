#pragma once

#include "fabric/core/BgfxHandle.hh"

#include <bgfx/bgfx.h>
#include <cstdint>

namespace fabric {

// Panini projection post-processing pass.
// Reads scene color texture and applies cylindrical projection correction
// for high-FOV rendering (120-150 degrees) without edge distortion.
//
// Pipeline order: scene render -> Panini -> bloom -> tonemap
class PaniniPass {
  public:
    PaniniPass();
    ~PaniniPass();

    PaniniPass(const PaniniPass&) = delete;
    PaniniPass& operator=(const PaniniPass&) = delete;

    // Initialize shader program, uniforms, and fullscreen quad.
    // Requires live bgfx context.
    void init(uint16_t width, uint16_t height);

    void shutdown();

    bool isValid() const;

    // Recreate internal resources at new dimensions.
    void resize(uint16_t width, uint16_t height);

    // Execute the Panini projection pass.
    // Reads sceneColor texture, writes to output via viewId.
    // If disabled, simply samples sceneColor and passes through.
    void execute(bgfx::TextureHandle sceneColor, bgfx::ViewId viewId);

    // Configuration
    void setEnabled(bool enabled);
    void setStrength(float d);             // 0.0-1.0, default 0.5
    void setFOV(float degrees);            // default 150.0
    void setFill(float fill);              // 0.0-1.0, default 1.0
    void setVerticalCompensation(float s); // -1.0 to 1.0, default 0.0

    bool isEnabled() const;

    // Toggle enabled state
    void cycleEnabled();

    // Cycle through strength presets: 0.3 -> 0.5 -> 0.7 -> 1.0 -> 0.3
    void cycleStrength();

  private:
    void initPrograms();
    void createVertexBuffer();

    // Shader program
    BgfxHandle<bgfx::ProgramHandle> program_;

    // Fullscreen triangle vertex buffer
    BgfxHandle<bgfx::VertexBufferHandle> fullscreenQuad_;

    // Uniforms
    BgfxHandle<bgfx::UniformHandle> u_sceneTex_;     // sampler
    BgfxHandle<bgfx::UniformHandle> u_params_;       // vec4(strength, half_tan_fov, fill_zoom, enabled)
    BgfxHandle<bgfx::UniformHandle> u_viewportSize_; // vec4(width, height, 1/width, 1/height)
    BgfxHandle<bgfx::UniformHandle> u_paniniExtra_;  // vec4(vertical_comp, aspect, 0, 0)

    // Configuration state
    bool enabled_ = false;      // OFF by default, toggle with F6
    float strength_ = 0.5f;     // D parameter: 0=rectilinear, 1=full Panini
    float fovDeg_ = 150.0f;     // Vertical FOV in degrees
    float fill_ = 1.0f;         // Fill zoom to remove empty corners
    float verticalComp_ = 0.0f; // Vertical compensation factor

    // Dimensions
    uint16_t width_ = 0;
    uint16_t height_ = 0;

    bool initialized_ = false;
};

} // namespace fabric
