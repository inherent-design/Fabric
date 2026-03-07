#include "fabric/core/Camera.hh"
#include <bx/math.h>
#include <cstring>

namespace fabric {

Camera::Camera() {
    // Initialize matrices to identity
    bx::mtxIdentity(view_);
    bx::mtxIdentity(viewWorld_);
    bx::mtxIdentity(projection_);
}

void Camera::setPerspective(float fovYDeg, float aspect, float nearPlane, float farPlane, bool homogeneousNdc) {
    fovY_ = fovYDeg;
    aspect_ = aspect;
    near_ = nearPlane;
    far_ = farPlane;
    orthographic_ = false;
    bx::mtxProj(projection_, fovYDeg, aspect, nearPlane, farPlane, homogeneousNdc);
}

void Camera::setOrthographic(float left, float right, float bottom, float top, float nearPlane, float farPlane,
                             bool homogeneousNdc) {
    near_ = nearPlane;
    far_ = farPlane;
    orthographic_ = true;
    bx::mtxOrtho(projection_, left, right, bottom, top, nearPlane, farPlane, 0.0f, homogeneousNdc);
}

void Camera::updateView(const Transform<float>& transform) {
    auto pos = transform.getPosition();
    updateView(Vector3<double, Space::World>(static_cast<double>(pos.x), static_cast<double>(pos.y),
                                             static_cast<double>(pos.z)),
               transform.getRotation());
}

void Camera::updateView(const Vector3<double, Space::World>& worldPos, const Quaternion<float>& rotation) {
    worldPosD_ = worldPos;

    // Left-handed: forward is +Z
    auto fwd = rotation.rotateVector(Vector3<float, Space::World>(0.0f, 0.0f, 1.0f));
    auto up = rotation.rotateVector(Vector3<float, Space::World>(0.0f, 1.0f, 0.0f));

    // Camera-relative view for rendering submissions.
    bx::Vec3 relEye(0.0f, 0.0f, 0.0f);
    bx::Vec3 relAt(fwd.x, fwd.y, fwd.z);
    bx::Vec3 relUp(up.x, up.y, up.z);
    bx::mtxLookAt(view_, relEye, relAt, relUp);

    // World-space view for frustum extraction/culling.
    bx::Vec3 worldEye(static_cast<float>(worldPosD_.x), static_cast<float>(worldPosD_.y),
                      static_cast<float>(worldPosD_.z));
    bx::Vec3 worldAt(worldEye.x + fwd.x, worldEye.y + fwd.y, worldEye.z + fwd.z);
    bx::Vec3 worldUp(up.x, up.y, up.z);
    bx::mtxLookAt(viewWorld_, worldEye, worldAt, worldUp);
}

Vector3<float, Space::World> Camera::getPosition() const {
    return Vector3<float, Space::World>(static_cast<float>(worldPosD_.x), static_cast<float>(worldPosD_.y),
                                        static_cast<float>(worldPosD_.z));
}

const Vector3<double, Space::World>& Camera::worldPositionD() const {
    return worldPosD_;
}

Vector3<float, Space::World> Camera::cameraRelative(const Vector3<double, Space::World>& worldPos) const {
    return Vector3<float, Space::World>(static_cast<float>(worldPos.x - worldPosD_.x),
                                        static_cast<float>(worldPos.y - worldPosD_.y),
                                        static_cast<float>(worldPos.z - worldPosD_.z));
}

const float* Camera::viewMatrix() const {
    return view_;
}

const float* Camera::projectionMatrix() const {
    return projection_;
}

void Camera::getViewProjection(float* outVP) const {
    bx::mtxMul(outVP, viewWorld_, projection_);
}

float Camera::fovY() const {
    return fovY_;
}
float Camera::aspectRatio() const {
    return aspect_;
}
float Camera::nearPlane() const {
    return near_;
}
float Camera::farPlane() const {
    return far_;
}
bool Camera::isOrthographic() const {
    return orthographic_;
}

} // namespace fabric
