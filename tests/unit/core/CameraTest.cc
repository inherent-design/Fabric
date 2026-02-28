#include "fabric/core/Camera.hh"
#include <cmath>
#include <gtest/gtest.h>

using namespace fabric;

class CameraTest : public ::testing::Test {
  protected:
    Camera camera;
};

// Parameter accessors

TEST_F(CameraTest, DefaultParameters) {
    EXPECT_FLOAT_EQ(camera.fovY(), 60.0f);
    EXPECT_FLOAT_EQ(camera.aspectRatio(), 16.0f / 9.0f);
    EXPECT_FLOAT_EQ(camera.nearPlane(), 0.1f);
    EXPECT_FLOAT_EQ(camera.farPlane(), 1000.0f);
    EXPECT_FALSE(camera.isOrthographic());
}

TEST_F(CameraTest, SetPerspectiveUpdatesParameters) {
    camera.setPerspective(90.0f, 4.0f / 3.0f, 0.5f, 500.0f, true);

    EXPECT_FLOAT_EQ(camera.fovY(), 90.0f);
    EXPECT_FLOAT_EQ(camera.aspectRatio(), 4.0f / 3.0f);
    EXPECT_FLOAT_EQ(camera.nearPlane(), 0.5f);
    EXPECT_FLOAT_EQ(camera.farPlane(), 500.0f);
    EXPECT_FALSE(camera.isOrthographic());
}

TEST_F(CameraTest, SetOrthographicUpdatesFlag) {
    camera.setOrthographic(-10.0f, 10.0f, -10.0f, 10.0f, 0.1f, 100.0f, true);

    EXPECT_TRUE(camera.isOrthographic());
    EXPECT_FLOAT_EQ(camera.nearPlane(), 0.1f);
    EXPECT_FLOAT_EQ(camera.farPlane(), 100.0f);
}

// Default matrices should be identity

TEST_F(CameraTest, DefaultViewIsIdentity) {
    const float* v = camera.viewMatrix();
    EXPECT_FLOAT_EQ(v[0], 1.0f);
    EXPECT_FLOAT_EQ(v[5], 1.0f);
    EXPECT_FLOAT_EQ(v[10], 1.0f);
    EXPECT_FLOAT_EQ(v[15], 1.0f);
    // Off-diagonals should be zero
    EXPECT_FLOAT_EQ(v[1], 0.0f);
    EXPECT_FLOAT_EQ(v[4], 0.0f);
}

TEST_F(CameraTest, DefaultProjectionIsIdentity) {
    const float* p = camera.projectionMatrix();
    EXPECT_FLOAT_EQ(p[0], 1.0f);
    EXPECT_FLOAT_EQ(p[5], 1.0f);
    EXPECT_FLOAT_EQ(p[10], 1.0f);
    EXPECT_FLOAT_EQ(p[15], 1.0f);
}

// Perspective projection produces non-identity matrix

TEST_F(CameraTest, PerspectiveProducesNonIdentity) {
    camera.setPerspective(60.0f, 16.0f / 9.0f, 0.1f, 1000.0f, true);
    const float* p = camera.projectionMatrix();

    // p[0] should be the x-scale factor (non-zero, non-one for 60 FOV)
    EXPECT_NE(p[0], 0.0f);
    EXPECT_NE(p[0], 1.0f);
    // p[5] should be the y-scale factor
    EXPECT_NE(p[5], 0.0f);
    EXPECT_NE(p[5], 1.0f);
}

TEST_F(CameraTest, PerspectiveHomogeneousNdcAffectsMatrix) {
    Camera cam1, cam2;
    cam1.setPerspective(60.0f, 1.0f, 0.1f, 100.0f, true);
    cam2.setPerspective(60.0f, 1.0f, 0.1f, 100.0f, false);

    // The depth mapping differs between homogeneous NDC on/off.
    // Element [10] (row 2, col 2 in column-major) encodes the depth.
    const float* p1 = cam1.projectionMatrix();
    const float* p2 = cam2.projectionMatrix();
    EXPECT_NE(p1[10], p2[10]);
}

// Orthographic projection

TEST_F(CameraTest, OrthographicProducesExpectedScaling) {
    camera.setOrthographic(-10.0f, 10.0f, -5.0f, 5.0f, 0.0f, 100.0f, true);
    const float* p = camera.projectionMatrix();

    // For a symmetric ortho with range 20 on x, scale = 2/(right-left) = 0.1
    EXPECT_NEAR(p[0], 2.0f / 20.0f, 1e-5f);
    // For range 10 on y, scale = 2/(top-bottom) = 0.2
    EXPECT_NEAR(p[5], 2.0f / 10.0f, 1e-5f);
}

// View matrix from Transform

TEST_F(CameraTest, ViewMatrixFromIdentityTransform) {
    Transform<float> t;
    camera.updateView(t);
    const float* v = camera.viewMatrix();

    // Identity transform at origin looking forward (+Z in LH).
    // View matrix should place camera at origin. The translation column (12,13,14) should be near zero.
    EXPECT_NEAR(v[12], 0.0f, 1e-5f);
    EXPECT_NEAR(v[13], 0.0f, 1e-5f);
    EXPECT_NEAR(v[14], 0.0f, 1e-5f);
}

TEST_F(CameraTest, ViewMatrixFromTranslatedTransform) {
    Transform<float> t;
    t.setPosition(Vector3<float, Space::World>(5.0f, 3.0f, 0.0f));
    camera.updateView(t);
    const float* v = camera.viewMatrix();

    // The view matrix should encode the inverse of the camera position.
    // For a camera at (5,3,0) looking along +Z, the translation component
    // of the view matrix should negate the eye position (dot products with axes).
    // At minimum, the matrix should differ from identity.
    EXPECT_NE(v[12], 0.0f);
}

// VP multiplication

TEST_F(CameraTest, ViewProjectionMultiplication) {
    camera.setPerspective(60.0f, 1.0f, 0.1f, 100.0f, true);

    Transform<float> t;
    t.setPosition(Vector3<float, Space::World>(0.0f, 0.0f, -10.0f));
    camera.updateView(t);

    float vp[16];
    camera.getViewProjection(vp);

    // VP should not be zero or identity (both V and P are non-trivial)
    bool allZero = true;
    bool isIdentity = true;
    for (int i = 0; i < 16; ++i) {
        if (vp[i] != 0.0f)
            allZero = false;
        float expected = (i % 5 == 0) ? 1.0f : 0.0f;
        if (std::abs(vp[i] - expected) > 1e-5f)
            isIdentity = false;
    }
    EXPECT_FALSE(allZero);
    EXPECT_FALSE(isIdentity);
}

// Switching between perspective and orthographic

TEST_F(CameraTest, SwitchFromPerspectiveToOrthographic) {
    camera.setPerspective(60.0f, 1.0f, 0.1f, 100.0f, true);
    EXPECT_FALSE(camera.isOrthographic());

    camera.setOrthographic(-1.0f, 1.0f, -1.0f, 1.0f, 0.0f, 10.0f, true);
    EXPECT_TRUE(camera.isOrthographic());

    // Perspective element [11] is typically -1 (or +1); ortho should be 0
    const float* p = camera.projectionMatrix();
    EXPECT_NEAR(p[11], 0.0f, 1e-5f);
}
