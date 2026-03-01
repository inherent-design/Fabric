#include "fabric/core/CameraController.hh"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace fabric {

namespace {

constexpr float kDegToRad = std::numbers::pi_v<float> / 180.0f;
constexpr float kSpringArmClipOffset = 0.2f;

float wrapAngle(float degrees) {
    degrees = std::fmod(degrees, 360.0f);
    if (degrees < 0.0f)
        degrees += 360.0f;
    return degrees;
}

} // namespace

CameraController::CameraController(Camera& camera, const CameraConfig& config)
    : camera_(camera), config_(config), actualDistance_(config.orbitDistance) {}

void CameraController::setMode(CameraMode mode) {
    mode_ = mode;
    if (mode == CameraMode::ThirdPerson)
        actualDistance_ = config_.orbitDistance;
}

CameraMode CameraController::mode() const {
    return mode_;
}

void CameraController::processMouseInput(float deltaX, float deltaY) {
    yaw_ += deltaX * config_.mouseSensitivity / kDegToRad;
    pitch_ += deltaY * config_.mouseSensitivity / kDegToRad;
    wrapYaw();
    clampPitch();
}

void CameraController::update(const Vector3<float, Space::World>& targetPos, float dt, const ChunkedGrid<float>* grid,
                              float densityThreshold) {
    using Vec3 = Vector3<float, Space::World>;

    Vec3 eyePoint = targetPos + Vec3(0.0f, config_.eyeHeight, 0.0f);
    auto rot = buildRotation();
    // Left-handed: forward = +Z
    Vec3 fwd = rot.rotateVector(Vec3(0.0f, 0.0f, 1.0f));

    if (mode_ == CameraMode::FirstPerson) {
        cachedPosition_ = eyePoint;
    } else {
        // Third person: camera behind the player (opposite of forward)
        float targetDist = config_.orbitDistance;

        if (grid) {
            // Cast ray from eye point backward (negative forward) to find obstructions
            Vec3 rayDir = Vec3(0.0f, 0.0f, 0.0f) - fwd; // -forward
            auto hit = castRay(*grid, eyePoint.x, eyePoint.y, eyePoint.z, rayDir.x, rayDir.y, rayDir.z,
                               config_.orbitDistance, densityThreshold);
            if (hit && hit->t < targetDist) {
                targetDist = std::max(hit->t - kSpringArmClipOffset, config_.orbitMinDistance);
            }
        }

        // Smooth distance transition
        actualDistance_ += (targetDist - actualDistance_) * std::min(config_.springArmSmoothing * dt, 1.0f);
        actualDistance_ = std::max(actualDistance_, config_.orbitMinDistance);

        cachedPosition_ = eyePoint - fwd * actualDistance_;
    }

    // Build transform and update the underlying Camera
    Transform<float> camTransform;
    camTransform.setPosition(cachedPosition_);
    camTransform.setRotation(rot);
    camera_.updateView(camTransform);
}

Vector3<float, Space::World> CameraController::position() const {
    return cachedPosition_;
}

Vector3<float, Space::World> CameraController::forward() const {
    return buildRotation().rotateVector(Vector3<float, Space::World>(0.0f, 0.0f, 1.0f));
}

Vector3<float, Space::World> CameraController::right() const {
    return buildRotation().rotateVector(Vector3<float, Space::World>(1.0f, 0.0f, 0.0f));
}

Vector3<float, Space::World> CameraController::up() const {
    return buildRotation().rotateVector(Vector3<float, Space::World>(0.0f, 1.0f, 0.0f));
}

float CameraController::yaw() const {
    return yaw_;
}

float CameraController::pitch() const {
    return pitch_;
}

void CameraController::setYaw(float degrees) {
    yaw_ = degrees;
    wrapYaw();
}

void CameraController::setPitch(float degrees) {
    pitch_ = degrees;
    clampPitch();
}

void CameraController::setUnlockPitch(bool unlock) {
    config_.unlockPitch = unlock;
}

CameraConfig& CameraController::config() {
    return config_;
}

Quaternion<float> CameraController::buildRotation() const {
    // Yaw around Y axis, then pitch around X axis
    float yawRad = yaw_ * kDegToRad;
    float pitchRad = pitch_ * kDegToRad;

    auto yawQuat = Quaternion<float>::fromAxisAngle(Vector3<float, Space::World>(0.0f, 1.0f, 0.0f), yawRad);
    auto pitchQuat = Quaternion<float>::fromAxisAngle(Vector3<float, Space::World>(1.0f, 0.0f, 0.0f), pitchRad);

    return yawQuat * pitchQuat;
}

void CameraController::clampPitch() {
    if (config_.unlockPitch) {
        pitch_ = wrapAngle(pitch_);
    } else {
        pitch_ = std::clamp(pitch_, config_.pitchMin, config_.pitchMax);
    }
}

void CameraController::wrapYaw() {
    yaw_ = wrapAngle(yaw_);
}

} // namespace fabric
