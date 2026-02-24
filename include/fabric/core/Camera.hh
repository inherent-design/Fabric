#pragma once

#include "fabric/core/Spatial.hh"

namespace fabric {

// Standalone camera for bgfx view transform submission.
// Owns projection and view matrices as float[16] (bgfx-compatible).
// Uses bx::mtxProj / bx::mtxLookAt internally, NOT Spatial.hh projection.
class Camera {
  public:
    Camera();

    // Projection setup (homogeneousNdc: true for OpenGL/Vulkan, false for D3D/Metal)
    void setPerspective(float fovYDeg, float aspect, float nearPlane, float farPlane, bool homogeneousNdc);
    void setOrthographic(float left, float right, float bottom, float top, float nearPlane, float farPlane,
                         bool homogeneousNdc);

    // Update view matrix from a Transform (call each frame)
    void updateView(const Transform<float>& transform);

    // Access matrices (float[16], bgfx-compatible)
    const float* viewMatrix() const;
    const float* projectionMatrix() const;

    // Combined view-projection (for frustum extraction)
    void getViewProjection(float* outVP) const;

    // Projection parameters
    float fovY() const;
    float aspectRatio() const;
    float nearPlane() const;
    float farPlane() const;
    bool isOrthographic() const;

  private:
    float view_[16];
    float projection_[16];
    float fovY_ = 60.0f;
    float aspect_ = 16.0f / 9.0f;
    float near_ = 0.1f;
    float far_ = 1000.0f;
    bool orthographic_ = false;
};

} // namespace fabric
