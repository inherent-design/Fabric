#pragma once

#include "fabric/core/Camera.hh"
#include "fabric/core/ChunkedGrid.hh"
#include "fabric/core/Spatial.hh"
#include "fabric/core/VoxelRaycast.hh"

namespace fabric {

enum class CameraMode {
    FirstPerson,
    ThirdPerson
};

struct CameraConfig {
    float mouseSensitivity = 0.003f;
    float fovY = 60.0f;
    float nearPlane = 0.1f;
    float farPlane = 1000.0f;

    // Third-person
    float orbitDistance = 8.0f;
    float orbitMinDistance = 1.5f;
    float springArmSmoothing = 10.0f;

    // Eye offset from player position (first person)
    float eyeHeight = 1.6f;

    // Pitch limits (degrees)
    float pitchMin = -89.0f;
    float pitchMax = 89.0f;
    bool unlockPitch = false;
};

class CameraController {
  public:
    CameraController(Camera& camera, const CameraConfig& config = {});

    void setMode(CameraMode mode);
    CameraMode mode() const;

    void processMouseInput(float deltaX, float deltaY);

    void update(const Vector3<float, Space::World>& targetPos, float dt, const ChunkedGrid<float>* grid = nullptr,
                float densityThreshold = 0.5f);

    Vector3<float, Space::World> position() const;

    Vector3<float, Space::World> forward() const;
    Vector3<float, Space::World> right() const;
    Vector3<float, Space::World> up() const;

    float yaw() const;
    float pitch() const;
    void setYaw(float degrees);
    void setPitch(float degrees);

    void setUnlockPitch(bool unlock);

    CameraConfig& config();

  private:
    Quaternion<float> buildRotation() const;
    void clampPitch();
    void wrapYaw();

    Camera& camera_;
    CameraConfig config_;
    CameraMode mode_ = CameraMode::FirstPerson;

    float yaw_ = 0.0f;
    float pitch_ = 0.0f;
    float actualDistance_ = 0.0f;

    Vector3<float, Space::World> cachedPosition_;
};

} // namespace fabric
