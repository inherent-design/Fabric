#include "fabric/core/CameraController.hh"
#include <cmath>
#include <gtest/gtest.h>
#include <numbers>

using namespace fabric;

class CameraControllerTest : public ::testing::Test {
  protected:
    Camera camera;

    void SetUp() override {
        camera.setPerspective(60.0f, 16.0f / 9.0f, 0.1f, 1000.0f, true);
    }
};

// -- First person tests --

TEST_F(CameraControllerTest, FirstPersonPositionAtEyeHeight) {
    CameraController ctrl(camera);
    ctrl.setMode(CameraMode::FirstPerson);

    Vector3<float, Space::World> target(10.0f, 0.0f, 5.0f);
    ctrl.update(target, 0.016f);

    auto pos = ctrl.position();
    EXPECT_FLOAT_EQ(pos.x, 10.0f);
    EXPECT_FLOAT_EQ(pos.y, 1.6f); // default eyeHeight
    EXPECT_FLOAT_EQ(pos.z, 5.0f);
}

TEST_F(CameraControllerTest, FirstPersonMouseRotatesView) {
    CameraController ctrl(camera);
    ctrl.setMode(CameraMode::FirstPerson);

    float initialYaw = ctrl.yaw();
    float initialPitch = ctrl.pitch();

    ctrl.processMouseInput(100.0f, 50.0f);

    EXPECT_NE(ctrl.yaw(), initialYaw);
    EXPECT_NE(ctrl.pitch(), initialPitch);
}

TEST_F(CameraControllerTest, FirstPersonPitchClampedAt89) {
    CameraController ctrl(camera);
    ctrl.setMode(CameraMode::FirstPerson);

    // Push pitch way beyond limits
    ctrl.setPitch(100.0f);
    EXPECT_FLOAT_EQ(ctrl.pitch(), 89.0f);

    ctrl.setPitch(-100.0f);
    EXPECT_FLOAT_EQ(ctrl.pitch(), -89.0f);
}

// -- Third person tests --

TEST_F(CameraControllerTest, ThirdPersonCameraBehindPlayer) {
    CameraController ctrl(camera);
    ctrl.setMode(CameraMode::ThirdPerson);
    ctrl.setYaw(0.0f);
    ctrl.setPitch(0.0f);

    Vector3<float, Space::World> target(0.0f, 0.0f, 0.0f);
    // Run several frames to let spring arm converge
    for (int i = 0; i < 100; ++i)
        ctrl.update(target, 0.016f);

    auto pos = ctrl.position();
    // At yaw=0, pitch=0, forward = +Z, so camera should be behind = -Z
    EXPECT_NEAR(pos.x, 0.0f, 0.1f);
    EXPECT_NEAR(pos.y, 1.6f, 0.1f);
    EXPECT_LT(pos.z, 0.0f); // Behind the player
    EXPECT_NEAR(pos.z, -8.0f + 1.6f * 0.0f, 1.0f); // Roughly at -orbitDistance on Z
}

TEST_F(CameraControllerTest, SpringArmShortensOnCollision) {
    CameraConfig cfg;
    cfg.orbitDistance = 8.0f;
    cfg.springArmSmoothing = 1000.0f; // High smoothing for instant convergence
    CameraController ctrl(camera, cfg);
    ctrl.setMode(CameraMode::ThirdPerson);
    ctrl.setYaw(0.0f);
    ctrl.setPitch(0.0f);

    // Build a grid with a wall 3 units behind the player
    ChunkedGrid<float> grid;
    // Forward is +Z at yaw=0, so "behind" is -Z
    // Fill a wall at z=-3
    for (int x = -5; x <= 5; ++x) {
        for (int y = -5; y <= 15; ++y) {
            grid.set(x, y, -3, 1.0f);
        }
    }

    Vector3<float, Space::World> target(0.0f, 0.0f, 0.0f);
    for (int i = 0; i < 50; ++i)
        ctrl.update(target, 0.016f, &grid);

    auto pos = ctrl.position();
    // Camera should be closer than full orbit distance (8) due to wall at z=-3
    float dist = std::sqrt(pos.x * pos.x + (pos.y - 1.6f) * (pos.y - 1.6f) + pos.z * pos.z);
    EXPECT_LT(dist, 4.0f);
    EXPECT_GT(dist, cfg.orbitMinDistance - 0.1f);
}

TEST_F(CameraControllerTest, SpringArmReturnsToFullDistance) {
    CameraConfig cfg;
    cfg.springArmSmoothing = 50.0f;
    CameraController ctrl(camera, cfg);
    ctrl.setMode(CameraMode::ThirdPerson);
    ctrl.setYaw(0.0f);
    ctrl.setPitch(0.0f);

    // First: update with a wall nearby (simulate collision)
    ChunkedGrid<float> gridWithWall;
    for (int x = -5; x <= 5; ++x) {
        for (int y = -5; y <= 15; ++y) {
            gridWithWall.set(x, y, -2, 1.0f);
        }
    }

    Vector3<float, Space::World> target(0.0f, 0.0f, 0.0f);
    for (int i = 0; i < 50; ++i)
        ctrl.update(target, 0.016f, &gridWithWall);

    // Camera should be close
    auto closerPos = ctrl.position();
    float closerDist = std::abs(closerPos.z);

    // Now update without grid (no collision) and let spring arm recover
    for (int i = 0; i < 200; ++i)
        ctrl.update(target, 0.016f);

    auto farPos = ctrl.position();
    float farDist = std::abs(farPos.z);

    EXPECT_GT(farDist, closerDist);
}

// -- Mode switching --

TEST_F(CameraControllerTest, ModeSwitchFirstToThird) {
    CameraController ctrl(camera);
    EXPECT_EQ(ctrl.mode(), CameraMode::FirstPerson);

    ctrl.setMode(CameraMode::ThirdPerson);
    EXPECT_EQ(ctrl.mode(), CameraMode::ThirdPerson);
}

TEST_F(CameraControllerTest, ModeSwitchThirdToFirst) {
    CameraController ctrl(camera);
    ctrl.setMode(CameraMode::ThirdPerson);
    ctrl.setMode(CameraMode::FirstPerson);
    EXPECT_EQ(ctrl.mode(), CameraMode::FirstPerson);
}

// -- Direction vectors --

TEST_F(CameraControllerTest, DirectionVectorsOrthogonal) {
    CameraController ctrl(camera);
    ctrl.setYaw(45.0f);
    ctrl.setPitch(30.0f);

    auto fwd = ctrl.forward();
    auto rt = ctrl.right();
    auto u = ctrl.up();

    EXPECT_NEAR(fwd.dot(rt), 0.0f, 1e-5f);
    EXPECT_NEAR(fwd.dot(u), 0.0f, 1e-5f);
    EXPECT_NEAR(rt.dot(u), 0.0f, 1e-5f);
}

TEST_F(CameraControllerTest, DirectionVectorsUnitLength) {
    CameraController ctrl(camera);
    ctrl.setYaw(120.0f);
    ctrl.setPitch(-45.0f);

    EXPECT_NEAR(ctrl.forward().length(), 1.0f, 1e-5f);
    EXPECT_NEAR(ctrl.right().length(), 1.0f, 1e-5f);
    EXPECT_NEAR(ctrl.up().length(), 1.0f, 1e-5f);
}

// -- Yaw wrapping --

TEST_F(CameraControllerTest, YawWrapsAround360) {
    CameraController ctrl(camera);
    ctrl.setYaw(370.0f);
    EXPECT_NEAR(ctrl.yaw(), 10.0f, 1e-3f);

    ctrl.setYaw(-10.0f);
    EXPECT_NEAR(ctrl.yaw(), 350.0f, 1e-3f);
}

// -- Pitch unlock --

TEST_F(CameraControllerTest, PitchUnlockAllowsFullRotation) {
    CameraController ctrl(camera);
    ctrl.setUnlockPitch(true);

    ctrl.setPitch(100.0f);
    EXPECT_NEAR(ctrl.pitch(), 100.0f, 1e-3f);

    ctrl.setPitch(270.0f);
    EXPECT_NEAR(ctrl.pitch(), 270.0f, 1e-3f);
}

// -- Null grid skips collision --

TEST_F(CameraControllerTest, NullGridSkipsCollision) {
    CameraController ctrl(camera);
    ctrl.setMode(CameraMode::ThirdPerson);
    ctrl.setYaw(0.0f);
    ctrl.setPitch(0.0f);

    Vector3<float, Space::World> target(0.0f, 0.0f, 0.0f);
    // Should not crash with nullptr grid
    for (int i = 0; i < 50; ++i)
        ctrl.update(target, 0.016f, nullptr);

    auto pos = ctrl.position();
    // Camera should approach full orbit distance
    float dist = std::abs(pos.z);
    EXPECT_GT(dist, 5.0f);
}

// -- Custom eye height --

TEST_F(CameraControllerTest, CustomEyeHeight) {
    CameraConfig cfg;
    cfg.eyeHeight = 3.0f;
    CameraController ctrl(camera, cfg);
    ctrl.setMode(CameraMode::FirstPerson);

    Vector3<float, Space::World> target(0.0f, 0.0f, 0.0f);
    ctrl.update(target, 0.016f);

    EXPECT_FLOAT_EQ(ctrl.position().y, 3.0f);
}

// -- Forward direction matches yaw at zero pitch --

TEST_F(CameraControllerTest, ForwardMatchesYawAtZeroPitch) {
    CameraController ctrl(camera);
    ctrl.setYaw(0.0f);
    ctrl.setPitch(0.0f);

    auto fwd = ctrl.forward();
    // At yaw=0, pitch=0, left-handed: forward should be +Z
    EXPECT_NEAR(fwd.x, 0.0f, 1e-5f);
    EXPECT_NEAR(fwd.y, 0.0f, 1e-5f);
    EXPECT_NEAR(fwd.z, 1.0f, 1e-5f);
}
