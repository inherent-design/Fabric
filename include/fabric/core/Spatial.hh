#pragma once

#include <array>
#include <cmath>
#include <numbers>
#include <type_traits>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace fabric {

// Forward declarations
template <typename T, typename SpaceTag> class Vector2;
template <typename T, typename SpaceTag> class Vector3;
template <typename T, typename SpaceTag> class Vector4;
template <typename T> class Matrix4x4;
template <typename T> class Quaternion;
template <typename T> class Transform;

/**
 * @brief Type tags for different coordinate spaces
 *
 * These tags are used to distinguish between different coordinate spaces
 * at compile time, preventing accidental mixing of spaces.
 */
namespace Space {
struct Local {};  // Object's local coordinate space
struct World {};  // World-space coordinates
struct Screen {}; // Screen-space coordinates
struct Parent {}; // Parent-space coordinates
} // namespace Space

/**
 * @brief 2D vector class with coordinate space type safety
 *
 * @tparam T Numeric type (float, double, etc.)
 * @tparam Space Coordinate space tag
 */
template <typename T, typename SpaceTag = Space::World> class Vector2 {
  public:
    T x, y;

    Vector2() : x(0), y(0) {}
    Vector2(T x, T y) : x(x), y(y) {}

    // Operators with the same space
    Vector2<T, SpaceTag> operator+(const Vector2<T, SpaceTag>& other) const {
        return Vector2<T, SpaceTag>(x + other.x, y + other.y);
    }

    Vector2<T, SpaceTag> operator-(const Vector2<T, SpaceTag>& other) const {
        return Vector2<T, SpaceTag>(x - other.x, y - other.y);
    }

    Vector2<T, SpaceTag> operator*(T scalar) const { return Vector2<T, SpaceTag>(x * scalar, y * scalar); }

    Vector2<T, SpaceTag> operator/(T scalar) const { return Vector2<T, SpaceTag>(x / scalar, y / scalar); }

    // Cannot mix different spaces - these operations are deleted
    template <typename OtherSpace> Vector2<T, SpaceTag> operator+(const Vector2<T, OtherSpace>&) const = delete;

    template <typename OtherSpace> Vector2<T, SpaceTag> operator-(const Vector2<T, OtherSpace>&) const = delete;

    // Dot product
    T dot(const Vector2<T, SpaceTag>& other) const { return x * other.x + y * other.y; }

    // Length calculations
    T lengthSquared() const { return x * x + y * y; }

    T length() const { return std::sqrt(lengthSquared()); }

    // Normalization
    Vector2<T, SpaceTag> normalized() const {
        T len = length();
        if (len == 0)
            return *this;
        return *this / len;
    }

    void normalize() {
        T len = length();
        if (len == 0)
            return;
        x /= len;
        y /= len;
    }

    // Space conversion function
    template <typename TargetSpace> Vector2<T, TargetSpace> as() const { return Vector2<T, TargetSpace>(x, y); }
};

/**
 * @brief 3D vector class with coordinate space type safety
 *
 * @tparam T Numeric type (float, double, etc.)
 * @tparam Space Coordinate space tag
 */
template <typename T, typename SpaceTag = Space::World> class Vector3 {
  public:
    T x, y, z;

    Vector3() : x(0), y(0), z(0) {}
    Vector3(T x, T y, T z) : x(x), y(y), z(z) {}

    // Operators with the same space
    Vector3<T, SpaceTag> operator+(const Vector3<T, SpaceTag>& other) const {
        return Vector3<T, SpaceTag>(x + other.x, y + other.y, z + other.z);
    }

    Vector3<T, SpaceTag> operator-(const Vector3<T, SpaceTag>& other) const {
        return Vector3<T, SpaceTag>(x - other.x, y - other.y, z - other.z);
    }

    Vector3<T, SpaceTag> operator*(T scalar) const { return Vector3<T, SpaceTag>(x * scalar, y * scalar, z * scalar); }

    Vector3<T, SpaceTag> operator/(T scalar) const { return Vector3<T, SpaceTag>(x / scalar, y / scalar, z / scalar); }

    // Cannot mix different spaces - these operations are deleted
    template <typename OtherSpace> Vector3<T, SpaceTag> operator+(const Vector3<T, OtherSpace>&) const = delete;

    template <typename OtherSpace> Vector3<T, SpaceTag> operator-(const Vector3<T, OtherSpace>&) const = delete;

    // Dot product
    T dot(const Vector3<T, SpaceTag>& other) const { return x * other.x + y * other.y + z * other.z; }

    // Cross product
    Vector3<T, SpaceTag> cross(const Vector3<T, SpaceTag>& other) const {
        return Vector3<T, SpaceTag>(y * other.z - z * other.y, z * other.x - x * other.z, x * other.y - y * other.x);
    }

    // Length calculations
    T lengthSquared() const { return x * x + y * y + z * z; }

    T length() const { return std::sqrt(lengthSquared()); }

    // Normalization
    Vector3<T, SpaceTag> normalized() const {
        T len = length();
        if (len == 0)
            return *this;
        return *this / len;
    }

    void normalize() {
        T len = length();
        if (len == 0)
            return;
        x /= len;
        y /= len;
        z /= len;
    }

    // Linear interpolation
    static Vector3<T, SpaceTag> lerp(const Vector3<T, SpaceTag>& a, const Vector3<T, SpaceTag>& b, T t) {
        return Vector3<T, SpaceTag>(a.x + t * (b.x - a.x), a.y + t * (b.y - a.y), a.z + t * (b.z - a.z));
    }

    // Space conversion function
    template <typename TargetSpace> Vector3<T, TargetSpace> as() const { return Vector3<T, TargetSpace>(x, y, z); }
};

/**
 * @brief 4D vector class with coordinate space type safety
 *
 * @tparam T Numeric type (float, double, etc.)
 * @tparam Space Coordinate space tag
 */
template <typename T, typename SpaceTag = Space::World> class Vector4 {
  public:
    T x, y, z, w;

    Vector4() : x(0), y(0), z(0), w(0) {}
    Vector4(T x, T y, T z, T w) : x(x), y(y), z(z), w(w) {}
    Vector4(const Vector3<T, SpaceTag>& v, T w) : x(v.x), y(v.y), z(v.z), w(w) {}

    // Operators with the same space
    Vector4<T, SpaceTag> operator+(const Vector4<T, SpaceTag>& other) const {
        return Vector4<T, SpaceTag>(x + other.x, y + other.y, z + other.z, w + other.w);
    }

    Vector4<T, SpaceTag> operator-(const Vector4<T, SpaceTag>& other) const {
        return Vector4<T, SpaceTag>(x - other.x, y - other.y, z - other.z, w - other.w);
    }

    Vector4<T, SpaceTag> operator*(T scalar) const {
        return Vector4<T, SpaceTag>(x * scalar, y * scalar, z * scalar, w * scalar);
    }

    Vector4<T, SpaceTag> operator/(T scalar) const {
        return Vector4<T, SpaceTag>(x / scalar, y / scalar, z / scalar, w / scalar);
    }

    // Cannot mix different spaces - these operations are deleted
    template <typename OtherSpace> Vector4<T, SpaceTag> operator+(const Vector4<T, OtherSpace>&) const = delete;

    template <typename OtherSpace> Vector4<T, SpaceTag> operator-(const Vector4<T, OtherSpace>&) const = delete;

    // Dot product
    T dot(const Vector4<T, SpaceTag>& other) const { return x * other.x + y * other.y + z * other.z + w * other.w; }

    // Length calculations
    T lengthSquared() const { return x * x + y * y + z * z + w * w; }

    T length() const { return std::sqrt(lengthSquared()); }

    // Normalization
    Vector4<T, SpaceTag> normalized() const {
        T len = length();
        if (len == 0)
            return *this;
        return *this / len;
    }

    void normalize() {
        T len = length();
        if (len == 0)
            return;
        x /= len;
        y /= len;
        z /= len;
        w /= len;
    }

    // Conversion to Vector3 (drops w)
    Vector3<T, SpaceTag> xyz() const { return Vector3<T, SpaceTag>(x, y, z); }

    // Space conversion function
    template <typename TargetSpace> Vector4<T, TargetSpace> as() const { return Vector4<T, TargetSpace>(x, y, z, w); }
};

/**
 * @brief Quaternion class for representing rotations
 *
 * @tparam T Numeric type (float, double, etc.)
 */
template <typename T> class Quaternion {
  public:
    T x, y, z, w;

    Quaternion() : x(0), y(0), z(0), w(1) {}
    Quaternion(T x, T y, T z, T w) : x(x), y(y), z(z), w(w) {}

    // Create from axis angle
    static Quaternion<T> fromAxisAngle(const Vector3<T, Space::World>& axis, T angle) {
        T halfAngle = angle * T(0.5);
        T s = std::sin(halfAngle);

        return Quaternion<T>(axis.x * s, axis.y * s, axis.z * s, std::cos(halfAngle));
    }

    // Create from Euler angles (in radians)
    static Quaternion<T> fromEulerAngles(T pitch, T yaw, T roll) {
        T cy = std::cos(yaw * T(0.5));
        T sy = std::sin(yaw * T(0.5));
        T cp = std::cos(pitch * T(0.5));
        T sp = std::sin(pitch * T(0.5));
        T cr = std::cos(roll * T(0.5));
        T sr = std::sin(roll * T(0.5));

        return Quaternion<T>(cy * sp * cr + sy * cp * sr, cy * cp * sr - sy * sp * cr, sy * cp * cr - cy * sp * sr,
                             cy * cp * cr + sy * sp * sr);
    }

    // Quaternion multiplication
    Quaternion<T> operator*(const Quaternion<T>& other) const {
        return Quaternion<T>(w * other.x + x * other.w + y * other.z - z * other.y,
                             w * other.y - x * other.z + y * other.w + z * other.x,
                             w * other.z + x * other.y - y * other.x + z * other.w,
                             w * other.w - x * other.x - y * other.y - z * other.z);
    }

    // Length operations
    T lengthSquared() const { return x * x + y * y + z * z + w * w; }

    T length() const { return std::sqrt(lengthSquared()); }

    // Normalization
    Quaternion<T> normalized() const {
        T len = length();
        if (len == 0)
            return *this;
        return Quaternion<T>(x / len, y / len, z / len, w / len);
    }

    void normalize() {
        T len = length();
        if (len == 0)
            return;
        x /= len;
        y /= len;
        z /= len;
        w /= len;
    }

    // Conjugate
    Quaternion<T> conjugate() const { return Quaternion<T>(-x, -y, -z, w); }

    // Inverse
    Quaternion<T> inverse() const {
        T lenSq = lengthSquared();
        if (lenSq == 0)
            return *this;
        T invLenSq = T(1) / lenSq;
        return Quaternion<T>(-x * invLenSq, -y * invLenSq, -z * invLenSq, w * invLenSq);
    }

    // Spherical linear interpolation
    static Quaternion<T> slerp(const Quaternion<T>& a, const Quaternion<T>& b, T t) {
        T dot = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;

        // Negate one quaternion to take shorter path
        Quaternion<T> b2 = b;
        if (dot < T(0)) {
            dot = -dot;
            b2 = Quaternion<T>(-b.x, -b.y, -b.z, -b.w);
        }

        // Fall back to normalized lerp when quaternions are very close
        if (dot > T(0.9995)) {
            return Quaternion<T>(a.x + t * (b2.x - a.x), a.y + t * (b2.y - a.y), a.z + t * (b2.z - a.z),
                                 a.w + t * (b2.w - a.w))
                .normalized();
        }

        T theta = std::acos(dot);
        T sinTheta = std::sin(theta);
        T wa = std::sin((T(1) - t) * theta) / sinTheta;
        T wb = std::sin(t * theta) / sinTheta;

        return Quaternion<T>(wa * a.x + wb * b2.x, wa * a.y + wb * b2.y, wa * a.z + wb * b2.z, wa * a.w + wb * b2.w);
    }

    // Rotate a vector by this quaternion
    template <typename SpaceTag> Vector3<T, SpaceTag> rotateVector(const Vector3<T, SpaceTag>& v) const {
        Quaternion<T> vQuat(v.x, v.y, v.z, 0);
        Quaternion<T> result = *this * vQuat * conjugate();
        return Vector3<T, SpaceTag>(result.x, result.y, result.z);
    }

    // Convert to Matrix4x4
    Matrix4x4<T> toMatrix() const;

    // Convert to Euler angles (in radians)
    Vector3<T, Space::World> toEulerAngles() const {
        Vector3<T, Space::World> angles;

        // Roll (x-axis rotation)
        T sinr_cosp = 2 * (w * x + y * z);
        T cosr_cosp = 1 - 2 * (x * x + y * y);
        angles.x = std::atan2(sinr_cosp, cosr_cosp);

        // Pitch (y-axis rotation)
        T sinp = 2 * (w * y - z * x);
        if (std::abs(sinp) >= 1)
            angles.y = std::copysign(std::numbers::pi_v<T> / T(2), sinp);
        else
            angles.y = std::asin(sinp);

        // Yaw (z-axis rotation)
        T siny_cosp = 2 * (w * z + x * y);
        T cosy_cosp = 1 - 2 * (y * y + z * z);
        angles.z = std::atan2(siny_cosp, cosy_cosp);

        return angles;
    }
};

/**
 * @brief 4x4 matrix class for transformations
 *
 * @tparam T Numeric type (float, double, etc.)
 */
template <typename T> class Matrix4x4 {
  public:
    // Matrix stored in column-major order (OpenGL style)
    std::array<T, 16> elements;

    // Constructor - identity matrix by default
    Matrix4x4() { setIdentity(); }

    // Constructor from array
    explicit Matrix4x4(const std::array<T, 16>& data) : elements(data) {}

    // Set to identity matrix
    void setIdentity() { elements = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}; }

    // Element access
    T& operator()(int row, int col) { return elements[col * 4 + row]; }

    const T& operator()(int row, int col) const { return elements[col * 4 + row]; }

    // Matrix multiplication
    Matrix4x4<T> operator*(const Matrix4x4<T>& other) const {
        Matrix4x4<T> result;

        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                result(i, j) = 0;
                for (int k = 0; k < 4; ++k) {
                    result(i, j) += (*this)(i, k) * other(k, j);
                }
            }
        }

        return result;
    }

    // Vector multiplication (homogeneous coordinates)
    template <typename SpaceTag, typename ResultSpaceTag>
    Vector4<T, ResultSpaceTag> operator*(const Vector4<T, SpaceTag>& v) const {
        return Vector4<T, ResultSpaceTag>(
            elements[0] * v.x + elements[4] * v.y + elements[8] * v.z + elements[12] * v.w,
            elements[1] * v.x + elements[5] * v.y + elements[9] * v.z + elements[13] * v.w,
            elements[2] * v.x + elements[6] * v.y + elements[10] * v.z + elements[14] * v.w,
            elements[3] * v.x + elements[7] * v.y + elements[11] * v.z + elements[15] * v.w);
    }

    // Vector3 transformation with implicit w=1
    template <typename SpaceTag, typename ResultSpaceTag>
    Vector3<T, ResultSpaceTag> transformPoint(const Vector3<T, SpaceTag>& v) const {
        Vector4<T, ResultSpaceTag> result =
            this->template operator* <SpaceTag, ResultSpaceTag>(Vector4<T, SpaceTag>(v, 1));
        if (result.w != 0) {
            return Vector3<T, ResultSpaceTag>(result.x / result.w, result.y / result.w, result.z / result.w);
        } else {
            return Vector3<T, ResultSpaceTag>(result.x, result.y, result.z);
        }
    }

    // Vector3 transformation with implicit w=0 (direction vectors)
    template <typename SpaceTag, typename ResultSpaceTag>
    Vector3<T, ResultSpaceTag> transformDirection(const Vector3<T, SpaceTag>& v) const {
        Vector4<T, ResultSpaceTag> result =
            this->template operator* <SpaceTag, ResultSpaceTag>(Vector4<T, SpaceTag>(v, 0));
        return Vector3<T, ResultSpaceTag>(result.x, result.y, result.z);
    }

    // Create a translation matrix
    static Matrix4x4<T> translation(const Vector3<T, Space::World>& v) {
        Matrix4x4<T> result;
        result(0, 3) = v.x;
        result(1, 3) = v.y;
        result(2, 3) = v.z;
        return result;
    }

    // Create a scale matrix
    static Matrix4x4<T> scaling(const Vector3<T, Space::World>& v) {
        Matrix4x4<T> result;
        result(0, 0) = v.x;
        result(1, 1) = v.y;
        result(2, 2) = v.z;
        return result;
    }

    // Create a rotation matrix from quaternion
    static Matrix4x4<T> rotation(const Quaternion<T>& q) {
        T xx = q.x * q.x;
        T xy = q.x * q.y;
        T xz = q.x * q.z;
        T xw = q.x * q.w;
        T yy = q.y * q.y;
        T yz = q.y * q.z;
        T yw = q.y * q.w;
        T zz = q.z * q.z;
        T zw = q.z * q.w;

        Matrix4x4<T> result;
        result(0, 0) = 1 - 2 * (yy + zz);
        result(0, 1) = 2 * (xy - zw);
        result(0, 2) = 2 * (xz + yw);

        result(1, 0) = 2 * (xy + zw);
        result(1, 1) = 1 - 2 * (xx + zz);
        result(1, 2) = 2 * (yz - xw);

        result(2, 0) = 2 * (xz - yw);
        result(2, 1) = 2 * (yz + xw);
        result(2, 2) = 1 - 2 * (xx + yy);

        return result;
    }

    // Create a perspective projection matrix
    static Matrix4x4<T> perspective(T fovY, T aspect, T near, T far) {
        Matrix4x4<T> result;

        T f = T(1) / std::tan(fovY / T(2));

        result(0, 0) = f / aspect;
        result(1, 1) = f;
        result(2, 2) = (far + near) / (near - far);
        result(2, 3) = (T(2) * far * near) / (near - far);
        result(3, 2) = -T(1);
        result(3, 3) = T(0);

        return result;
    }

    // Create an orthographic projection matrix
    static Matrix4x4<T> orthographic(T left, T right, T bottom, T top, T near, T far) {
        Matrix4x4<T> result;

        result(0, 0) = T(2) / (right - left);
        result(1, 1) = T(2) / (top - bottom);
        result(2, 2) = T(2) / (near - far);

        result(0, 3) = (left + right) / (left - right);
        result(1, 3) = (bottom + top) / (bottom - top);
        result(2, 3) = (near + far) / (near - far);

        return result;
    }

    // Create a look-at view matrix
    static Matrix4x4<T> lookAt(const Vector3<T, Space::World>& eye, const Vector3<T, Space::World>& target,
                               const Vector3<T, Space::World>& up) {
        Vector3<T, Space::World> f = (target - eye).normalized();
        Vector3<T, Space::World> s = f.cross(up).normalized();
        Vector3<T, Space::World> u = s.cross(f);

        Matrix4x4<T> result;

        result(0, 0) = s.x;
        result(0, 1) = s.y;
        result(0, 2) = s.z;

        result(1, 0) = u.x;
        result(1, 1) = u.y;
        result(1, 2) = u.z;

        result(2, 0) = -f.x;
        result(2, 1) = -f.y;
        result(2, 2) = -f.z;

        result(0, 3) = -s.dot(eye);
        result(1, 3) = -u.dot(eye);
        result(2, 3) = f.dot(eye);

        return result;
    }

    Matrix4x4<T> inverse() const {
        static_assert(std::is_same_v<T, float> || std::is_same_v<T, double>,
                      "Matrix4x4::inverse() only supported for float and double");

        if constexpr (std::is_same_v<T, float>) {
            glm::mat4 glmMat = glm::make_mat4(elements.data());
            float det = glm::determinant(glmMat);
            if (std::abs(det) < 1e-8f) {
                return Matrix4x4<T>();
            }
            glm::mat4 glmInv = glm::inverse(glmMat);
            Matrix4x4<T> result;
            std::copy(glm::value_ptr(glmInv), glm::value_ptr(glmInv) + 16, result.elements.begin());
            return result;
        } else {
            glm::dmat4 glmMat = glm::make_mat4(elements.data());
            double det = glm::determinant(glmMat);
            if (std::abs(det) < 1e-15) {
                return Matrix4x4<T>();
            }
            glm::dmat4 glmInv = glm::inverse(glmMat);
            Matrix4x4<T> result;
            std::copy(glm::value_ptr(glmInv), glm::value_ptr(glmInv) + 16, result.elements.begin());
            return result;
        }
    }

    // Matrix transpose
    Matrix4x4<T> transpose() const {
        Matrix4x4<T> result;
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                result(i, j) = (*this)(j, i);
            }
        }
        return result;
    }
};

// Implementation of Quaternion::toMatrix now that Matrix4x4 is defined
template <typename T> Matrix4x4<T> Quaternion<T>::toMatrix() const {
    return Matrix4x4<T>::rotation(*this);
}

/**
 * @brief Transform class for handling position, rotation, and scale
 *
 * @tparam T Numeric type (float, double, etc.)
 */
template <typename T> class Transform {
  public:
    using Vec3 = Vector3<T, Space::World>;
    using Quat = Quaternion<T>;
    using Mat4 = Matrix4x4<T>;

    Transform() : position_(Vec3(0, 0, 0)), rotation_(Quat()), scale_(Vec3(1, 1, 1)), dirty_(true) {}

    // Get components
    const Vec3& getPosition() const { return position_; }
    const Quat& getRotation() const { return rotation_; }
    const Vec3& getScale() const { return scale_; }

    // Set components
    void setPosition(const Vec3& position) {
        position_ = position;
        dirty_ = true;
    }

    void setRotation(const Quat& rotation) {
        rotation_ = rotation;
        dirty_ = true;
    }

    void setScale(const Vec3& scale) {
        scale_ = scale;
        dirty_ = true;
    }

    // Set from euler angles (in radians)
    void setRotationEuler(T pitch, T yaw, T roll) {
        rotation_ = Quat::fromEulerAngles(pitch, yaw, roll);
        dirty_ = true;
    }

    // Set from axis angle
    void setRotationAxisAngle(const Vec3& axis, T angle) {
        rotation_ = Quat::fromAxisAngle(axis, angle);
        dirty_ = true;
    }

    // Get the transformation matrix
    const Mat4& getMatrix() const {
        if (dirty_) {
            updateMatrix();
        }
        return matrix_;
    }

    // Transform a point from local to world space
    template <typename SpaceTag> Vector3<T, Space::World> transformPoint(const Vector3<T, SpaceTag>& point) const {
        return getMatrix().template transformPoint<SpaceTag, Space::World>(point);
    }

    // Transform a direction from local to world space
    template <typename SpaceTag>
    Vector3<T, Space::World> transformDirection(const Vector3<T, SpaceTag>& direction) const {
        return getMatrix().template transformDirection<SpaceTag, Space::World>(direction);
    }

    // Combine two transforms
    Transform<T> operator*(const Transform<T>& other) const {
        Transform<T> result;
        result.matrix_ = getMatrix() * other.getMatrix();
        result.dirty_ = false;

        // Extract position from column 3 of the combined matrix
        const auto& m = result.matrix_;
        result.position_ = Vec3(m(0, 3), m(1, 3), m(2, 3));

        // Extract scale as column vector lengths
        T sx = std::sqrt(m(0, 0) * m(0, 0) + m(1, 0) * m(1, 0) + m(2, 0) * m(2, 0));
        T sy = std::sqrt(m(0, 1) * m(0, 1) + m(1, 1) * m(1, 1) + m(2, 1) * m(2, 1));
        T sz = std::sqrt(m(0, 2) * m(0, 2) + m(1, 2) * m(1, 2) + m(2, 2) * m(2, 2));
        result.scale_ = Vec3(sx, sy, sz);

        // Extract rotation: divide upper-left 3x3 by scale, then convert to quaternion
        if (sx > 0 && sy > 0 && sz > 0) {
            T r00 = m(0, 0) / sx, r01 = m(0, 1) / sy, r02 = m(0, 2) / sz;
            T r10 = m(1, 0) / sx, r11 = m(1, 1) / sy, r12 = m(1, 2) / sz;
            T r20 = m(2, 0) / sx, r21 = m(2, 1) / sy, r22 = m(2, 2) / sz;

            T trace = r00 + r11 + r22;
            if (trace > 0) {
                T s = T(0.5) / std::sqrt(trace + T(1));
                result.rotation_ = Quat((r21 - r12) * s, (r02 - r20) * s, (r10 - r01) * s, T(0.25) / s);
            } else if (r00 > r11 && r00 > r22) {
                T s = T(2) * std::sqrt(T(1) + r00 - r11 - r22);
                result.rotation_ = Quat(T(0.25) * s, (r01 + r10) / s, (r02 + r20) / s, (r21 - r12) / s);
            } else if (r11 > r22) {
                T s = T(2) * std::sqrt(T(1) + r11 - r00 - r22);
                result.rotation_ = Quat((r01 + r10) / s, T(0.25) * s, (r12 + r21) / s, (r02 - r20) / s);
            } else {
                T s = T(2) * std::sqrt(T(1) + r22 - r00 - r11);
                result.rotation_ = Quat((r02 + r20) / s, (r12 + r21) / s, T(0.25) * s, (r10 - r01) / s);
            }
        }

        return result;
    }

    // Combine this transform with another, resulting in a new transform
    Transform<T> combine(const Transform<T>& other) const {
        Transform<T> result;

        // Simple combination for position, scale, and rotation
        // Element-wise multiplication for scale
        Vec3 scaledPos = Vec3(other.position_.x * this->scale_.x, other.position_.y * this->scale_.y,
                              other.position_.z * this->scale_.z);
        result.position_ = this->position_ + this->rotation_.rotateVector(scaledPos);

        // Element-wise multiplication for scale
        result.scale_ =
            Vec3(this->scale_.x * other.scale_.x, this->scale_.y * other.scale_.y, this->scale_.z * other.scale_.z);

        result.rotation_ = this->rotation_ * other.rotation_;
        result.dirty_ = true;

        return result;
    }

  private:
    Vec3 position_;
    Quat rotation_;
    Vec3 scale_;
    mutable Mat4 matrix_;
    mutable bool dirty_;

    void updateMatrix() const {
        // Build the transformation matrix from components
        Mat4 translationMatrix = Mat4::translation(position_);
        Mat4 rotationMatrix = Mat4::rotation(rotation_);
        Mat4 scaleMatrix = Mat4::scaling(scale_);

        // Combine the transformations
        matrix_ = translationMatrix * rotationMatrix * scaleMatrix;
        dirty_ = false;
    }
};

} // namespace fabric
