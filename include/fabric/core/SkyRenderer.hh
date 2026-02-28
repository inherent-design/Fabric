#pragma once

#include "fabric/core/Spatial.hh"
#include <bgfx/bgfx.h>

namespace fabric {

// Renders a procedural atmospheric sky using a fullscreen triangle.
// Implements a Preetham/Perez luminance distribution driven by sun position.
// Call init() after bgfx::init() to create the shader program. render() is
// always safe to call -- it is a no-op until init() succeeds.
class SkyRenderer {
  public:
    SkyRenderer();
    ~SkyRenderer();

    // Create shader program and vertex buffer. Requires live bgfx context.
    void init();

    void shutdown();

    SkyRenderer(const SkyRenderer&) = delete;
    SkyRenderer& operator=(const SkyRenderer&) = delete;

    // Render the sky dome as a fullscreen triangle on the given view.
    // No-op if init() has not been called or failed.
    // Submits with color-only state (no depth write/test) so geometry
    // rendered after this naturally occludes the sky.
    void render(bgfx::ViewId view);

    // Set the sun direction (normalized, pointing toward the sun).
    void setSunDirection(const Vector3<float, Space::World>& dir);
    Vector3<float, Space::World> sunDirection() const;

    // Check if the shader program handle is valid
    bool isValid() const;

  private:
    void initProgram();

    bgfx::ProgramHandle program_;
    bgfx::VertexBufferHandle vbh_;
    bgfx::UniformHandle uniformSunDir_;
    bgfx::UniformHandle uniformParams_;
    Vector3<float, Space::World> sunDir_;
    float turbidity_ = 2.0f;
    bool initialized_ = false;
};

} // namespace fabric
