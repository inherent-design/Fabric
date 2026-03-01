#include "fabric/core/Camera.hh"
#include <bx/math.h>
#include <cstring>

namespace fabric {

Camera::Camera() {
    // Initialize both matrices to identity
    bx::mtxIdentity(view_);
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

    // Left-handed: forward is +Z
    auto fwd = transform.getRotation().rotateVector(Vector3<float, Space::World>(0.0f, 0.0f, 1.0f));
    auto up = transform.getRotation().rotateVector(Vector3<float, Space::World>(0.0f, 1.0f, 0.0f));

    bx::Vec3 eye(pos.x, pos.y, pos.z);
    bx::Vec3 at(pos.x + fwd.x, pos.y + fwd.y, pos.z + fwd.z);
    bx::Vec3 upVec(up.x, up.y, up.z);

    bx::mtxLookAt(view_, eye, at, upVec);
}

Vector3<float, Space::World> Camera::getPosition() const {
    // View matrix V = [R | t] where the camera position p satisfies t = -R*p.
    // So p = -R^T * t. R is the upper-left 3x3, t is column 3.
    // Column-major: element(row, col) = view_[col * 4 + row]
    float rx = view_[0], ry = view_[4], rz = view_[8];
    float ux = view_[1], uy = view_[5], uz = view_[9];
    float fx = view_[2], fy = view_[6], fz = view_[10];
    float tx = view_[12], ty = view_[13], tz = view_[14];

    // p = -R^T * t  (R^T rows are columns of R in the view matrix)
    return Vector3<float, Space::World>(-(rx * tx + ux * ty + fx * tz), -(ry * tx + uy * ty + fy * tz),
                                        -(rz * tx + uz * ty + fz * tz));
}

const float* Camera::viewMatrix() const {
    return view_;
}

const float* Camera::projectionMatrix() const {
    return projection_;
}

void Camera::getViewProjection(float* outVP) const {
    bx::mtxMul(outVP, view_, projection_);
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
