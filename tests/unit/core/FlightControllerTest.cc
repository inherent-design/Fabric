#include <gtest/gtest.h>
#include "fabric/core/FlightController.hh"
#include <cmath>

using namespace fabric;

class FlightControllerTest : public ::testing::Test {
  protected:
    // 1x2x1 character
    FlightController controller{1.0f, 2.0f, 1.0f};
    ChunkedGrid<float> grid;

    void fillSlab(int y, int xMin, int xMax, int zMin, int zMax) {
        for (int z = zMin; z <= zMax; ++z)
            for (int x = xMin; x <= xMax; ++x)
                grid.set(x, y, z, 1.0f);
    }
};

TEST_F(FlightControllerTest, FlyForwardEmptySpace) {
    Vec3f pos(0.0f, 5.0f, 0.0f);
    Vec3f disp(0.0f, 0.0f, 1.0f);

    auto result = controller.move(pos, disp, grid);
    EXPECT_FALSE(result.hitX);
    EXPECT_FALSE(result.hitY);
    EXPECT_FALSE(result.hitZ);
    EXPECT_NEAR(result.resolvedPosition.z, 1.0f, 0.01f);
}

TEST_F(FlightControllerTest, FlyUpEmptySpace) {
    Vec3f pos(0.0f, 5.0f, 0.0f);
    Vec3f disp(0.0f, 2.0f, 0.0f);

    auto result = controller.move(pos, disp, grid);
    EXPECT_NEAR(result.resolvedPosition.y, 7.0f, 0.01f);
}

TEST_F(FlightControllerTest, CollideWithWallX) {
    // Wall at x=3, character half-width=0.5
    grid.set(3, 5, 0, 1.0f);
    grid.set(3, 6, 0, 1.0f);

    // pos.x=2.5 + disp 0.5 = 3.0, AABB max.x=3.5 overlaps voxel at x=3
    Vec3f pos(2.5f, 5.0f, 0.0f);
    Vec3f disp(0.5f, 0.0f, 0.0f);

    auto result = controller.move(pos, disp, grid);
    EXPECT_TRUE(result.hitX);
    EXPECT_NEAR(result.resolvedPosition.x, 2.5f, 0.01f);
}

TEST_F(FlightControllerTest, CollideWithCeiling) {
    // Ceiling at y=10, character height=2.0
    fillSlab(10, -5, 5, -5, 5);

    // pos.y=8.0, AABB max.y=10.0. disp.y=0.5 puts max.y=10.5 overlapping y=10
    Vec3f pos(0.0f, 8.0f, 0.0f);
    Vec3f disp(0.0f, 0.5f, 0.0f);

    auto result = controller.move(pos, disp, grid);
    EXPECT_TRUE(result.hitY);
    EXPECT_NEAR(result.resolvedPosition.y, 8.0f, 0.01f);
}

TEST_F(FlightControllerTest, CollideWithFloor) {
    fillSlab(3, -5, 5, -5, 5);

    // pos.y=4.0 (feet at 4), disp.y=-1.5 puts feet at 2.5, overlaps voxel at y=3
    Vec3f pos(0.0f, 4.0f, 0.0f);
    Vec3f disp(0.0f, -1.5f, 0.0f);

    auto result = controller.move(pos, disp, grid);
    EXPECT_TRUE(result.hitY);
    EXPECT_NEAR(result.resolvedPosition.y, 4.0f, 0.01f);
}

TEST_F(FlightControllerTest, SlideAlongWall) {
    // Wall along X at z=3
    for (int x = -5; x <= 5; ++x) {
        grid.set(x, 5, 3, 1.0f);
        grid.set(x, 6, 3, 1.0f);
    }

    // Diagonal into wall: Z blocked, X passes
    Vec3f pos(0.0f, 5.0f, 2.5f);
    Vec3f disp(1.0f, 0.0f, 1.0f);

    auto result = controller.move(pos, disp, grid);
    EXPECT_TRUE(result.hitZ);
    EXPECT_FALSE(result.hitX);
    EXPECT_GT(result.resolvedPosition.x, 0.0f);
    EXPECT_NEAR(result.resolvedPosition.z, 2.5f, 0.01f);
}

TEST_F(FlightControllerTest, HoverInPlace) {
    Vec3f pos(0.0f, 5.0f, 0.0f);
    Vec3f disp(0.0f, 0.0f, 0.0f);

    auto result = controller.move(pos, disp, grid);
    EXPECT_NEAR(result.resolvedPosition.x, 0.0f, 0.001f);
    EXPECT_NEAR(result.resolvedPosition.y, 5.0f, 0.001f);
    EXPECT_NEAR(result.resolvedPosition.z, 0.0f, 0.001f);
}

TEST_F(FlightControllerTest, DiagonalResolvedPerAxis) {
    Vec3f pos(0.0f, 5.0f, 0.0f);
    Vec3f disp(1.0f, 0.5f, -0.5f);

    auto result = controller.move(pos, disp, grid);
    EXPECT_NEAR(result.resolvedPosition.x, 1.0f, 0.01f);
    EXPECT_NEAR(result.resolvedPosition.y, 5.5f, 0.01f);
    EXPECT_NEAR(result.resolvedPosition.z, -0.5f, 0.01f);
}

TEST_F(FlightControllerTest, NegativeCoordinates) {
    for (int x = -10; x <= -5; ++x)
        for (int z = -10; z <= -5; ++z)
            grid.set(x, -1, z, 1.0f);

    Vec3f pos(-7.0f, 0.0f, -7.0f);
    Vec3f disp(1.0f, 0.0f, 1.0f);

    auto result = controller.move(pos, disp, grid);
    EXPECT_NEAR(result.resolvedPosition.x, -6.0f, 0.01f);
    EXPECT_NEAR(result.resolvedPosition.z, -6.0f, 0.01f);
}

TEST_F(FlightControllerTest, AABBCorrect) {
    Vec3f pos(5.0f, 3.0f, 5.0f);
    auto box = controller.getAABB(pos);

    EXPECT_NEAR(box.min.x, 4.5f, 0.001f);
    EXPECT_NEAR(box.min.y, 3.0f, 0.001f);
    EXPECT_NEAR(box.min.z, 4.5f, 0.001f);
    EXPECT_NEAR(box.max.x, 5.5f, 0.001f);
    EXPECT_NEAR(box.max.y, 5.0f, 0.001f);
    EXPECT_NEAR(box.max.z, 5.5f, 0.001f);
}

// Drag utility tests

TEST_F(FlightControllerTest, DragReducesVelocity) {
    Vec3f vel(10.0f, 0.0f, 0.0f);
    float drag = 5.0f;
    float dt = 1.0f / 60.0f;

    Vec3f result = FlightController::applyDrag(vel, drag, dt);
    float speed = std::sqrt(result.x * result.x + result.y * result.y + result.z * result.z);
    EXPECT_LT(speed, 10.0f);
    EXPECT_GT(speed, 0.0f);
}

TEST_F(FlightControllerTest, DragClampsNearZero) {
    Vec3f vel(0.005f, 0.0f, 0.0f);
    float drag = 5.0f;
    float dt = 1.0f / 60.0f;

    Vec3f result = FlightController::applyDrag(vel, drag, dt);
    EXPECT_NEAR(result.x, 0.0f, 0.001f);
    EXPECT_NEAR(result.y, 0.0f, 0.001f);
    EXPECT_NEAR(result.z, 0.0f, 0.001f);
}

TEST_F(FlightControllerTest, DragZeroCoeffPreservesVelocity) {
    Vec3f vel(10.0f, 5.0f, -3.0f);

    Vec3f result = FlightController::applyDrag(vel, 0.0f, 1.0f / 60.0f);
    EXPECT_NEAR(result.x, 10.0f, 0.001f);
    EXPECT_NEAR(result.y, 5.0f, 0.001f);
    EXPECT_NEAR(result.z, -3.0f, 0.001f);
}

TEST_F(FlightControllerTest, DragLargeDtClampsToZero) {
    Vec3f vel(10.0f, 5.0f, 0.0f);
    // drag * dt > 1.0 should clamp factor to 0
    Vec3f result = FlightController::applyDrag(vel, 10.0f, 1.0f);
    EXPECT_NEAR(result.x, 0.0f, 0.001f);
    EXPECT_NEAR(result.y, 0.0f, 0.001f);
}
