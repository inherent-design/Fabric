#include "recurse/gameplay/CameraController.hh"
#include "fabric/simulation/SimulationGrid.hh"

#include <algorithm>
#include <cmath>
#include <numbers>

using namespace fabric;

namespace recurse {

namespace {

constexpr float K_DEG_TO_RAD = std::numbers::pi_v<float> / 180.0f;
constexpr float K_SPRING_ARM_CLIP_OFFSET = 0.2f;

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
    yaw_ += deltaX * config_.mouseSensitivity / K_DEG_TO_RAD;
    pitch_ += deltaY * config_.mouseSensitivity / K_DEG_TO_RAD;
    wrapYaw();
    clampPitch();
}

void CameraController::update(const Vector3<float, Space::World>& targetPos, float dt, const ChunkedGrid<float>* grid,
                              float densityThreshold) {
    update(Vector3<double, Space::World>(static_cast<double>(targetPos.x), static_cast<double>(targetPos.y),
                                         static_cast<double>(targetPos.z)),
           dt, grid, densityThreshold);
}

void CameraController::update(const Vector3<double, Space::World>& targetPos, float dt, const ChunkedGrid<float>* grid,
                              float densityThreshold) {
    using Vec3f = Vector3<float, Space::World>;

    const auto eyePointD = targetPos + Vector3<double, Space::World>(0.0, static_cast<double>(config_.eyeHeight), 0.0);
    auto rot = buildRotation();
    // Left-handed: forward = +Z
    Vec3f fwd = rot.rotateVector(Vec3f(0.0f, 0.0f, 1.0f));

    if (mode_ == CameraMode::FirstPerson) {
        cachedPositionD_ = eyePointD;
    } else {
        // Third person: camera behind the player (opposite of forward)
        float targetDist = config_.orbitDistance;

        if (grid) {
            // Cast ray from eye point backward (negative forward) to find obstructions
            Vec3f rayDir = Vec3f(0.0f, 0.0f, 0.0f) - fwd; // -forward
            auto hit = castRay(*grid, static_cast<float>(eyePointD.x), static_cast<float>(eyePointD.y),
                               static_cast<float>(eyePointD.z), rayDir.x, rayDir.y, rayDir.z, config_.orbitDistance,
                               densityThreshold);
            if (hit && hit->t < targetDist) {
                targetDist = std::max(hit->t - K_SPRING_ARM_CLIP_OFFSET, config_.orbitMinDistance);
            }
        }

        // Smooth distance transition
        actualDistance_ += (targetDist - actualDistance_) * std::min(config_.springArmSmoothing * dt, 1.0f);
        actualDistance_ = std::max(actualDistance_, config_.orbitMinDistance);

        cachedPositionD_ = eyePointD - Vector3<double, Space::World>(static_cast<double>(fwd.x) * actualDistance_,
                                                                     static_cast<double>(fwd.y) * actualDistance_,
                                                                     static_cast<double>(fwd.z) * actualDistance_);
    }

    camera_.updateView(cachedPositionD_, rot);
}

const Vector3<double, Space::World>& CameraController::positionD() const {
    return cachedPositionD_;
}

Vector3<float, Space::World> CameraController::position() const {
    return Vector3<float, Space::World>(static_cast<float>(cachedPositionD_.x), static_cast<float>(cachedPositionD_.y),
                                        static_cast<float>(cachedPositionD_.z));
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
    float yawRad = yaw_ * K_DEG_TO_RAD;
    float pitchRad = pitch_ * K_DEG_TO_RAD;

    auto yawQuat = Quaternion<float>::fromAxisAngle(Vector3<float, Space::World>(0.0f, 1.0f, 0.0f), yawRad);
    auto pitchQuat = Quaternion<float>::fromAxisAngle(Vector3<float, Space::World>(1.0f, 0.0f, 0.0f), pitchRad);

    return yawQuat * pitchQuat;
}

void CameraController::update(const Vector3<float, Space::World>& targetPos, float dt,
                              const fabric::simulation::SimulationGrid* grid) {
    update(Vector3<double, Space::World>(static_cast<double>(targetPos.x), static_cast<double>(targetPos.y),
                                         static_cast<double>(targetPos.z)),
           dt, grid);
}

void CameraController::update(const Vector3<double, Space::World>& targetPos, float dt,
                              const fabric::simulation::SimulationGrid* grid) {
    using Vec3f = Vector3<float, Space::World>;

    const auto eyePointD = targetPos + Vector3<double, Space::World>(0.0, static_cast<double>(config_.eyeHeight), 0.0);
    auto rot = buildRotation();
    Vec3f fwd = rot.rotateVector(Vec3f(0.0f, 0.0f, 1.0f));

    if (mode_ == CameraMode::FirstPerson) {
        cachedPositionD_ = eyePointD;
    } else {
        float targetDist = config_.orbitDistance;

        if (grid) {
            Vec3f rayDir = Vec3f(0.0f, 0.0f, 0.0f) - fwd;
            auto hit = castRay(*grid, static_cast<float>(eyePointD.x), static_cast<float>(eyePointD.y),
                               static_cast<float>(eyePointD.z), rayDir.x, rayDir.y, rayDir.z, config_.orbitDistance);
            if (hit && hit->t < targetDist) {
                targetDist = std::max(hit->t - K_SPRING_ARM_CLIP_OFFSET, config_.orbitMinDistance);
            }
        }

        actualDistance_ += (targetDist - actualDistance_) * std::min(config_.springArmSmoothing * dt, 1.0f);
        actualDistance_ = std::max(actualDistance_, config_.orbitMinDistance);

        cachedPositionD_ = eyePointD - Vector3<double, Space::World>(static_cast<double>(fwd.x) * actualDistance_,
                                                                     static_cast<double>(fwd.y) * actualDistance_,
                                                                     static_cast<double>(fwd.z) * actualDistance_);
    }

    camera_.updateView(cachedPositionD_, rot);
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

} // namespace recurse
