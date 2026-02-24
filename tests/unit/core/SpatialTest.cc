#include "fabric/core/Spatial.hh"
#include "fabric/utils/Testing.hh"
#include "fabric/utils/ErrorHandling.hh"
#include <gtest/gtest.h>
#include <cmath>
#include <numbers>

using namespace fabric;

// Helper for comparing floating point values
template <typename T>
bool almostEqual(T a, T b, T epsilon = static_cast<T>(1e-5)) {
    return std::abs(a - b) <= epsilon;
}

class SpatialTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Common setup
    }
};

TEST_F(SpatialTest, Vector2Basics) {
    Vector2<float, Space::World> v1(1.0f, 2.0f);
    
    // Test getters
    EXPECT_FLOAT_EQ(v1.x, 1.0f);
    EXPECT_FLOAT_EQ(v1.y, 2.0f);
    
    // Test operators
    Vector2<float, Space::World> v2(3.0f, 4.0f);
    Vector2<float, Space::World> sum = v1 + v2;
    EXPECT_FLOAT_EQ(sum.x, 4.0f);
    EXPECT_FLOAT_EQ(sum.y, 6.0f);
    
    Vector2<float, Space::World> diff = v2 - v1;
    EXPECT_FLOAT_EQ(diff.x, 2.0f);
    EXPECT_FLOAT_EQ(diff.y, 2.0f);
    
    Vector2<float, Space::World> scaled = v1 * 2.0f;
    EXPECT_FLOAT_EQ(scaled.x, 2.0f);
    EXPECT_FLOAT_EQ(scaled.y, 4.0f);
    
    Vector2<float, Space::World> divided = v2 / 2.0f;
    EXPECT_FLOAT_EQ(divided.x, 1.5f);
    EXPECT_FLOAT_EQ(divided.y, 2.0f);
}

TEST_F(SpatialTest, Vector2MathOperations) {
    Vector2<float, Space::World> v1(3.0f, 4.0f);
    
    // Test length
    EXPECT_FLOAT_EQ(v1.length(), 5.0f);
    EXPECT_FLOAT_EQ(v1.lengthSquared(), 25.0f);
    
    // Test normalization
    Vector2<float, Space::World> normalized = v1.normalized();
    EXPECT_FLOAT_EQ(normalized.length(), 1.0f);
    EXPECT_FLOAT_EQ(normalized.x, 0.6f);
    EXPECT_FLOAT_EQ(normalized.y, 0.8f);
    
    // Test dot product
    Vector2<float, Space::World> v2(1.0f, 2.0f);
    float dot = v1.dot(v2);
    EXPECT_FLOAT_EQ(dot, 11.0f); // 3*1 + 4*2
}

TEST_F(SpatialTest, Vector3Basics) {
    Vector3<float, Space::World> v1(1.0f, 2.0f, 3.0f);
    
    // Test getters
    EXPECT_FLOAT_EQ(v1.x, 1.0f);
    EXPECT_FLOAT_EQ(v1.y, 2.0f);
    EXPECT_FLOAT_EQ(v1.z, 3.0f);
    
    // Test operators
    Vector3<float, Space::World> v2(4.0f, 5.0f, 6.0f);
    Vector3<float, Space::World> sum = v1 + v2;
    EXPECT_FLOAT_EQ(sum.x, 5.0f);
    EXPECT_FLOAT_EQ(sum.y, 7.0f);
    EXPECT_FLOAT_EQ(sum.z, 9.0f);
    
    Vector3<float, Space::World> diff = v2 - v1;
    EXPECT_FLOAT_EQ(diff.x, 3.0f);
    EXPECT_FLOAT_EQ(diff.y, 3.0f);
    EXPECT_FLOAT_EQ(diff.z, 3.0f);
}

TEST_F(SpatialTest, Vector3MathOperations) {
    Vector3<float, Space::World> v1(2.0f, 3.0f, 4.0f);
    
    // Test length
    EXPECT_FLOAT_EQ(v1.length(), sqrtf(29.0f));
    EXPECT_FLOAT_EQ(v1.lengthSquared(), 29.0f);
    
    // Test normalization
    Vector3<float, Space::World> normalized = v1.normalized();
    EXPECT_FLOAT_EQ(normalized.length(), 1.0f);
    
    // Test dot product
    Vector3<float, Space::World> v2(1.0f, 2.0f, 3.0f);
    float dot = v1.dot(v2);
    EXPECT_FLOAT_EQ(dot, 20.0f); // 2*1 + 3*2 + 4*3
    
    // Test cross product
    Vector3<float, Space::World> cross = v1.cross(v2);
    EXPECT_FLOAT_EQ(cross.x, 1.0f);  // 3*3 - 4*2
    EXPECT_FLOAT_EQ(cross.y, -2.0f); // 4*1 - 2*3
    EXPECT_FLOAT_EQ(cross.z, 1.0f);  // 2*2 - 3*1
}

TEST_F(SpatialTest, Vector4Basics) {
    Vector4<float, Space::World> v1(1.0f, 2.0f, 3.0f, 4.0f);
    
    // Test getters
    EXPECT_FLOAT_EQ(v1.x, 1.0f);
    EXPECT_FLOAT_EQ(v1.y, 2.0f);
    EXPECT_FLOAT_EQ(v1.z, 3.0f);
    EXPECT_FLOAT_EQ(v1.w, 4.0f);
    
    // Test operators
    Vector4<float, Space::World> v2(5.0f, 6.0f, 7.0f, 8.0f);
    Vector4<float, Space::World> sum = v1 + v2;
    EXPECT_FLOAT_EQ(sum.x, 6.0f);
    EXPECT_FLOAT_EQ(sum.y, 8.0f);
    EXPECT_FLOAT_EQ(sum.z, 10.0f);
    EXPECT_FLOAT_EQ(sum.w, 12.0f);
    
    Vector4<float, Space::World> diff = v2 - v1;
    EXPECT_FLOAT_EQ(diff.x, 4.0f);
    EXPECT_FLOAT_EQ(diff.y, 4.0f);
    EXPECT_FLOAT_EQ(diff.z, 4.0f);
    EXPECT_FLOAT_EQ(diff.w, 4.0f);
}

TEST_F(SpatialTest, TypedCoordinateSafety) {
    // Create vectors in different spaces
    Vector3<float, Space::World> worldPos(1.0f, 2.0f, 3.0f);
    Vector3<float, Space::Local> localPos(4.0f, 5.0f, 6.0f);
    
    // Same space operations should work
    Vector3<float, Space::World> worldPos2(7.0f, 8.0f, 9.0f);
    Vector3<float, Space::World> worldSum = worldPos + worldPos2;
    EXPECT_FLOAT_EQ(worldSum.x, 8.0f);
    EXPECT_FLOAT_EQ(worldSum.y, 10.0f);
    EXPECT_FLOAT_EQ(worldSum.z, 12.0f);
    
    // Different space operations should not compile (tested manually)
    // Uncomment to test compilation error:
    // Vector3<float, Space::World> invalid = worldPos + localPos; // Should not compile
}

TEST_F(SpatialTest, QuaternionBasics) {
    Quaternion<float> q;
    
    // Default constructor should create identity quaternion
    EXPECT_FLOAT_EQ(q.x, 0.0f);
    EXPECT_FLOAT_EQ(q.y, 0.0f);
    EXPECT_FLOAT_EQ(q.z, 0.0f);
    EXPECT_FLOAT_EQ(q.w, 1.0f);
    
    // Test explicit constructor
    Quaternion<float> q2(0.5f, 0.5f, 0.5f, 0.5f);
    EXPECT_FLOAT_EQ(q2.x, 0.5f);
    EXPECT_FLOAT_EQ(q2.y, 0.5f);
    EXPECT_FLOAT_EQ(q2.z, 0.5f);
    EXPECT_FLOAT_EQ(q2.w, 0.5f);
}

TEST_F(SpatialTest, QuaternionRotation) {
    // Create a quaternion for 90 degrees rotation around Z axis
    Vector3<float, Space::World> axis(0.0f, 0.0f, 1.0f);
    float angle = std::numbers::pi / 2; // 90 degrees
    Quaternion<float> qRot = Quaternion<float>::fromAxisAngle(axis, angle);
    
    // Rotate a vector with this quaternion (should rotate x into y)
    Vector3<float, Space::World> v(1.0f, 0.0f, 0.0f);
    Vector3<float, Space::World> rotated = qRot.rotateVector(v);
    
    // Should be approximately (0, 1, 0)
    EXPECT_TRUE(almostEqual(rotated.x, 0.0f));
    EXPECT_TRUE(almostEqual(rotated.y, 1.0f));
    EXPECT_TRUE(almostEqual(rotated.z, 0.0f));
}

TEST_F(SpatialTest, QuaternionOperations) {
    Quaternion<float> q1(1.0f, 2.0f, 3.0f, 4.0f);
    q1 = q1.normalized(); // Normalize for proper rotation quaternion
    
    // Test length
    EXPECT_FLOAT_EQ(q1.length(), 1.0f);
    
    // Test conjugate
    Quaternion<float> conj = q1.conjugate();
    EXPECT_FLOAT_EQ(conj.x, -q1.x);
    EXPECT_FLOAT_EQ(conj.y, -q1.y);
    EXPECT_FLOAT_EQ(conj.z, -q1.z);
    EXPECT_FLOAT_EQ(conj.w, q1.w);
    
    // Test inverse
    Quaternion<float> inv = q1.inverse();
    Quaternion<float> identity = q1 * inv;
    EXPECT_TRUE(almostEqual(identity.x, 0.0f));
    EXPECT_TRUE(almostEqual(identity.y, 0.0f));
    EXPECT_TRUE(almostEqual(identity.z, 0.0f));
    EXPECT_TRUE(almostEqual(identity.w, 1.0f));
}

TEST_F(SpatialTest, Matrix4x4Basics) {
    Matrix4x4<float> identity;
    
    // Check identity
    EXPECT_FLOAT_EQ(identity(0, 0), 1.0f);
    EXPECT_FLOAT_EQ(identity(1, 1), 1.0f);
    EXPECT_FLOAT_EQ(identity(2, 2), 1.0f);
    EXPECT_FLOAT_EQ(identity(3, 3), 1.0f);
    
    EXPECT_FLOAT_EQ(identity(0, 1), 0.0f);
    EXPECT_FLOAT_EQ(identity(0, 2), 0.0f);
    EXPECT_FLOAT_EQ(identity(0, 3), 0.0f);
    
    // Test translation matrix creation
    Matrix4x4<float> translation = Matrix4x4<float>::translation(Vector3<float, Space::World>(10.0f, 20.0f, 30.0f));
    EXPECT_FLOAT_EQ(translation(0, 3), 10.0f);
    EXPECT_FLOAT_EQ(translation(1, 3), 20.0f);
    EXPECT_FLOAT_EQ(translation(2, 3), 30.0f);
    
    // Test scale matrix creation
    Matrix4x4<float> scale = Matrix4x4<float>::scaling(Vector3<float, Space::World>(2.0f, 3.0f, 4.0f));
    EXPECT_FLOAT_EQ(scale(0, 0), 2.0f);
    EXPECT_FLOAT_EQ(scale(1, 1), 3.0f);
    EXPECT_FLOAT_EQ(scale(2, 2), 4.0f);
}

TEST_F(SpatialTest, Matrix4x4Transformations) {
    // Create a translation matrix
    Matrix4x4<float> translation = Matrix4x4<float>::translation(Vector3<float, Space::World>(1.0f, 2.0f, 3.0f));
    
    // Create a point and transform it
    Vector3<float, Space::World> point(5.0f, 6.0f, 7.0f);
    Vector3<float, Space::World> transformed = translation.template transformPoint<Space::World, Space::World>(point);
    
    // Point should be translated
    EXPECT_FLOAT_EQ(transformed.x, 6.0f);  // 5 + 1
    EXPECT_FLOAT_EQ(transformed.y, 8.0f);  // 6 + 2
    EXPECT_FLOAT_EQ(transformed.z, 10.0f); // 7 + 3
    
    // Create a direction and transform it
    Vector3<float, Space::World> direction(1.0f, 0.0f, 0.0f);
    Vector3<float, Space::World> transformedDir = translation.template transformDirection<Space::World, Space::World>(direction);
    
    // Direction should remain unchanged by translation
    EXPECT_FLOAT_EQ(transformedDir.x, 1.0f);
    EXPECT_FLOAT_EQ(transformedDir.y, 0.0f);
    EXPECT_FLOAT_EQ(transformedDir.z, 0.0f);
}

TEST_F(SpatialTest, Matrix4x4Operations) {
    // Create matrices
    Matrix4x4<float> scaleMatrix = Matrix4x4<float>::scaling(Vector3<float, Space::World>(2.0f, 3.0f, 4.0f));
    Matrix4x4<float> translationMatrix = Matrix4x4<float>::translation(Vector3<float, Space::World>(5.0f, 6.0f, 7.0f));
    
    // Multiply matrices
    Matrix4x4<float> combined = translationMatrix * scaleMatrix;
    
    // Transform a point with combined matrix
    Vector3<float, Space::World> point(1.0f, 1.0f, 1.0f);
    Vector3<float, Space::World> transformed = combined.template transformPoint<Space::World, Space::World>(point);
    
    // Should scale first, then translate
    EXPECT_FLOAT_EQ(transformed.x, 7.0f);  // 1*2 + 5
    EXPECT_FLOAT_EQ(transformed.y, 9.0f);  // 1*3 + 6
    EXPECT_FLOAT_EQ(transformed.z, 11.0f); // 1*4 + 7
}

TEST_F(SpatialTest, TransformBasics) {
    Transform<float> transform;
    
    // Default transform should be identity
    Vector3<float, Space::World> defaultPos = transform.getPosition();
    Quaternion<float> defaultRot = transform.getRotation();
    Vector3<float, Space::World> defaultScale = transform.getScale();
    
    EXPECT_FLOAT_EQ(defaultPos.x, 0.0f);
    EXPECT_FLOAT_EQ(defaultPos.y, 0.0f);
    EXPECT_FLOAT_EQ(defaultPos.z, 0.0f);
    
    EXPECT_FLOAT_EQ(defaultRot.x, 0.0f);
    EXPECT_FLOAT_EQ(defaultRot.y, 0.0f);
    EXPECT_FLOAT_EQ(defaultRot.z, 0.0f);
    EXPECT_FLOAT_EQ(defaultRot.w, 1.0f);
    
    EXPECT_FLOAT_EQ(defaultScale.x, 1.0f);
    EXPECT_FLOAT_EQ(defaultScale.y, 1.0f);
    EXPECT_FLOAT_EQ(defaultScale.z, 1.0f);
    
    // Set custom transform
    Vector3<float, Space::World> position(1.0f, 2.0f, 3.0f);
    Vector3<float, Space::World> rotAxis(0.0f, 1.0f, 0.0f);
    Quaternion<float> rotation = Quaternion<float>::fromAxisAngle(rotAxis, std::numbers::pi / 2);
    Vector3<float, Space::World> scale(2.0f, 2.0f, 2.0f);
    
    transform.setPosition(position);
    transform.setRotation(rotation);
    transform.setScale(scale);
    
    // Check that the values were set correctly
    EXPECT_TRUE(almostEqual(transform.getPosition().x, position.x));
    EXPECT_TRUE(almostEqual(transform.getPosition().y, position.y));
    EXPECT_TRUE(almostEqual(transform.getPosition().z, position.z));
    
    EXPECT_TRUE(almostEqual(transform.getRotation().x, rotation.x));
    EXPECT_TRUE(almostEqual(transform.getRotation().y, rotation.y));
    EXPECT_TRUE(almostEqual(transform.getRotation().z, rotation.z));
    EXPECT_TRUE(almostEqual(transform.getRotation().w, rotation.w));
    
    EXPECT_TRUE(almostEqual(transform.getScale().x, scale.x));
    EXPECT_TRUE(almostEqual(transform.getScale().y, scale.y));
    EXPECT_TRUE(almostEqual(transform.getScale().z, scale.z));
}

TEST_F(SpatialTest, TransformPointAndDirection) {
    // Create a transform that scales, rotates, and translates
    Vector3<float, Space::World> position(0.0f, 1.0f, 0.0f);
    Vector3<float, Space::World> rotAxis(0.0f, 0.0f, 1.0f);
    Quaternion<float> rotation = Quaternion<float>::fromAxisAngle(rotAxis, std::numbers::pi / 2); // 90 degrees around Z
    Vector3<float, Space::World> scale(2.0f, 2.0f, 2.0f);
    
    Transform<float> transform;
    transform.setPosition(position);
    transform.setRotation(rotation);
    transform.setScale(scale);
    
    // Transform a point
    Vector3<float, Space::World> point(1.0f, 0.0f, 0.0f);
    Vector3<float, Space::World> transformedPoint = transform.transformPoint(point);
    
    // Point should be scaled, rotated, then translated
    // Scale: (2, 0, 0)
    // Rotate: (0, 2, 0) (90 degrees around Z)
    // Translate: (0, 3, 0)
    EXPECT_TRUE(almostEqual(transformedPoint.x, 0.0f));
    EXPECT_TRUE(almostEqual(transformedPoint.y, 3.0f));
    EXPECT_TRUE(almostEqual(transformedPoint.z, 0.0f));
    
    // Transform a direction
    Vector3<float, Space::World> direction(1.0f, 0.0f, 0.0f);
    Vector3<float, Space::World> transformedDirection = transform.transformDirection(direction);
    
    // Direction should be scaled and rotated, but not translated
    // Scale: (2, 0, 0)
    // Rotate: (0, 2, 0)
    EXPECT_TRUE(almostEqual(transformedDirection.x, 0.0f));
    EXPECT_TRUE(almostEqual(transformedDirection.y, 2.0f));
    EXPECT_TRUE(almostEqual(transformedDirection.z, 0.0f));
}


TEST_F(SpatialTest, QuaternionSlerp) {
    // Slerp with identical quaternions returns the same quaternion
    Quaternion<float> q1;
    auto result = Quaternion<float>::slerp(q1, q1, 0.5f);
    EXPECT_TRUE(almostEqual(result.x, q1.x));
    EXPECT_TRUE(almostEqual(result.y, q1.y));
    EXPECT_TRUE(almostEqual(result.z, q1.z));
    EXPECT_TRUE(almostEqual(result.w, q1.w));

    // Endpoints: t=0 returns a, t=1 returns b
    Vector3<float, Space::World> axis(0.0f, 0.0f, 1.0f);
    Quaternion<float> a = Quaternion<float>::fromAxisAngle(axis, 0.0f);
    Quaternion<float> b = Quaternion<float>::fromAxisAngle(axis, static_cast<float>(std::numbers::pi) / 2.0f);

    auto at0 = Quaternion<float>::slerp(a, b, 0.0f);
    EXPECT_TRUE(almostEqual(at0.x, a.x));
    EXPECT_TRUE(almostEqual(at0.y, a.y));
    EXPECT_TRUE(almostEqual(at0.z, a.z));
    EXPECT_TRUE(almostEqual(at0.w, a.w));

    auto at1 = Quaternion<float>::slerp(a, b, 1.0f);
    EXPECT_TRUE(almostEqual(at1.x, b.x));
    EXPECT_TRUE(almostEqual(at1.y, b.y));
    EXPECT_TRUE(almostEqual(at1.z, b.z));
    EXPECT_TRUE(almostEqual(at1.w, b.w));

    // Midpoint: t=0.5 for 90-degree rotation should give 45-degree rotation
    auto mid = Quaternion<float>::slerp(a, b, 0.5f);
    Quaternion<float> expected45 = Quaternion<float>::fromAxisAngle(axis, static_cast<float>(std::numbers::pi) / 4.0f);
    EXPECT_TRUE(almostEqual(mid.x, expected45.x));
    EXPECT_TRUE(almostEqual(mid.y, expected45.y));
    EXPECT_TRUE(almostEqual(mid.z, expected45.z));
    EXPECT_TRUE(almostEqual(mid.w, expected45.w));
}

TEST_F(SpatialTest, Vector3Lerp) {
    Vector3<float, Space::World> a(0.0f, 0.0f, 0.0f);
    Vector3<float, Space::World> b(10.0f, 20.0f, 30.0f);

    // Identity: lerp(a, a, t) == a
    auto same = Vector3<float, Space::World>::lerp(a, a, 0.5f);
    EXPECT_FLOAT_EQ(same.x, 0.0f);
    EXPECT_FLOAT_EQ(same.y, 0.0f);
    EXPECT_FLOAT_EQ(same.z, 0.0f);

    // Endpoints
    auto at0 = Vector3<float, Space::World>::lerp(a, b, 0.0f);
    EXPECT_FLOAT_EQ(at0.x, 0.0f);
    EXPECT_FLOAT_EQ(at0.y, 0.0f);
    EXPECT_FLOAT_EQ(at0.z, 0.0f);

    auto at1 = Vector3<float, Space::World>::lerp(a, b, 1.0f);
    EXPECT_FLOAT_EQ(at1.x, 10.0f);
    EXPECT_FLOAT_EQ(at1.y, 20.0f);
    EXPECT_FLOAT_EQ(at1.z, 30.0f);

    // Midpoint
    auto mid = Vector3<float, Space::World>::lerp(a, b, 0.5f);
    EXPECT_FLOAT_EQ(mid.x, 5.0f);
    EXPECT_FLOAT_EQ(mid.y, 10.0f);
    EXPECT_FLOAT_EQ(mid.z, 15.0f);
}

