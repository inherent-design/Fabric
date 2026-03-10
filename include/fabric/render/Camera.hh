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

    // Update view from explicit world position (double precision) and rotation.
    void updateView(const Vector3<double, Space::World>& worldPos, const Quaternion<float>& rotation);

    // Access matrices (float[16], bgfx-compatible)
    const float* viewMatrix() const;
    const float* projectionMatrix() const;

    // Combined view-projection (for frustum extraction)
    void getViewProjection(float* outVP) const;

    // World-space camera position (authoritative double, float convenience)
    Vector3<float, Space::World> getPosition() const;
    const Vector3<double, Space::World>& worldPositionD() const;
    Vector3<float, Space::World> cameraRelative(const Vector3<double, Space::World>& worldPos) const;

    // Projection parameters
    float fovY() const;
    float aspectRatio() const;
    float nearPlane() const;
    float farPlane() const;
    bool isOrthographic() const;

  private:
    // Camera-relative view used for GPU rendering submissions.
    float view_[16];
    // World-space view used for world-space culling and VP queries.
    float viewWorld_[16];
    float projection_[16];
    float fovY_ = 60.0f;
    float aspect_ = 16.0f / 9.0f;
    float near_ = 0.1f;
    float far_ = 1000.0f;
    bool orthographic_ = false;
    Vector3<double, Space::World> worldPosD_{0.0, 0.0, 0.0};
};

} // namespace fabric
