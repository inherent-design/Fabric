#include <gtest/gtest.h>
#include "fabric/core/CharacterController.hh"

using namespace fabric;

class CharacterControllerTest : public ::testing::Test {
  protected:
    // 1x2x1 character (width, height, depth)
    CharacterController controller{1.0f, 2.0f, 1.0f};
    ChunkedGrid<float> grid;

    void fillFloor(int y, int xMin, int xMax, int zMin, int zMax) {
        for (int z = zMin; z <= zMax; ++z)
            for (int x = xMin; x <= xMax; ++x)
                grid.set(x, y, z, 1.0f);
    }
};

TEST_F(CharacterControllerTest, WalkOnFlatSurface) {
    // Flat floor at y=0, character stands at y=1
    fillFloor(0, -5, 5, -5, 5);
    Vec3f pos(0.0f, 1.0f, 0.0f);
    Vec3f move(1.0f, 0.0f, 0.0f);

    auto result = controller.move(pos, move, grid);
    EXPECT_FALSE(result.hitX);
    EXPECT_FALSE(result.hitZ);
    EXPECT_TRUE(result.onGround);
    EXPECT_NEAR(result.resolvedPosition.x, 1.0f, 0.01f);
    EXPECT_NEAR(result.resolvedPosition.y, 1.0f, 0.01f);
}

TEST_F(CharacterControllerTest, CollideWithWallX) {
    fillFloor(0, -5, 5, -5, 5);
    // Wall at x=3, y=1..2
    grid.set(3, 1, 0, 1.0f);
    grid.set(3, 2, 0, 1.0f);

    Vec3f pos(2.0f, 1.0f, 0.0f);
    Vec3f move(2.0f, 0.0f, 0.0f);

    auto result = controller.move(pos, move, grid);
    EXPECT_TRUE(result.hitX);
    EXPECT_NEAR(result.resolvedPosition.x, 2.0f, 0.01f);
}

TEST_F(CharacterControllerTest, FallOffEdge) {
    // Floor from x=-5 to x=2 only
    fillFloor(0, -5, 2, -5, 5);

    Vec3f pos(2.0f, 1.0f, 0.0f);
    Vec3f move(2.0f, 0.0f, 0.0f);

    auto result = controller.move(pos, move, grid);
    EXPECT_FALSE(result.onGround);
    EXPECT_NEAR(result.resolvedPosition.x, 4.0f, 0.01f);
}

TEST_F(CharacterControllerTest, StepUpOntoOneLedge) {
    fillFloor(0, -5, 5, -5, 5);
    // 1-block ledge at x=3 y=1
    grid.set(3, 1, 0, 1.0f);

    Vec3f pos(2.0f, 1.0f, 0.0f);
    Vec3f move(2.0f, 0.0f, 0.0f);

    auto result = controller.move(pos, move, grid);
    // Should step up by 1
    EXPECT_FALSE(result.hitX);
    EXPECT_NEAR(result.resolvedPosition.y, 2.0f, 0.01f);
    EXPECT_NEAR(result.resolvedPosition.x, 4.0f, 0.01f);
}

TEST_F(CharacterControllerTest, CannotStepUpTwoBlockWall) {
    fillFloor(0, -5, 5, -5, 5);
    // 2-block wall at x=3 y=1,2
    grid.set(3, 1, 0, 1.0f);
    grid.set(3, 2, 0, 1.0f);

    Vec3f pos(2.0f, 1.0f, 0.0f);
    Vec3f move(2.0f, 0.0f, 0.0f);

    auto result = controller.move(pos, move, grid);
    EXPECT_TRUE(result.hitX);
    EXPECT_NEAR(result.resolvedPosition.x, 2.0f, 0.01f);
}

TEST_F(CharacterControllerTest, GravityPullsDown) {
    // No floor, character in air
    Vec3f pos(0.0f, 5.0f, 0.0f);
    Vec3f move(0.0f, -2.0f, 0.0f);

    auto result = controller.move(pos, move, grid);
    EXPECT_FALSE(result.onGround);
    EXPECT_NEAR(result.resolvedPosition.y, 3.0f, 0.01f);
}

TEST_F(CharacterControllerTest, GravityStopsOnGround) {
    fillFloor(0, -5, 5, -5, 5);
    Vec3f pos(0.0f, 2.0f, 0.0f);
    Vec3f move(0.0f, -3.0f, 0.0f);

    auto result = controller.move(pos, move, grid);
    EXPECT_TRUE(result.onGround);
    EXPECT_TRUE(result.hitY);
    EXPECT_NEAR(result.resolvedPosition.y, 1.0f, 0.01f);
}

TEST_F(CharacterControllerTest, NegativeCoordinates) {
    // Floor at y=-1 in negative space
    for (int x = -10; x <= -5; ++x)
        for (int z = -10; z <= -5; ++z)
            grid.set(x, -1, z, 1.0f);

    Vec3f pos(-7.0f, 0.0f, -7.0f);
    Vec3f move(1.0f, 0.0f, 1.0f);

    auto result = controller.move(pos, move, grid);
    EXPECT_TRUE(result.onGround);
    EXPECT_NEAR(result.resolvedPosition.x, -6.0f, 0.01f);
    EXPECT_NEAR(result.resolvedPosition.z, -6.0f, 0.01f);
}

TEST_F(CharacterControllerTest, CollideWithWallZ) {
    fillFloor(0, -5, 5, -5, 5);
    // Wall at z=3, y=1..2
    grid.set(0, 1, 3, 1.0f);
    grid.set(0, 2, 3, 1.0f);

    Vec3f pos(0.0f, 1.0f, 2.0f);
    Vec3f move(0.0f, 0.0f, 2.0f);

    auto result = controller.move(pos, move, grid);
    EXPECT_TRUE(result.hitZ);
    EXPECT_NEAR(result.resolvedPosition.z, 2.0f, 0.01f);
}

TEST_F(CharacterControllerTest, AABBCorrect) {
    Vec3f pos(5.0f, 3.0f, 5.0f);
    auto box = controller.getAABB(pos);

    EXPECT_NEAR(box.min.x, 4.5f, 0.001f);
    EXPECT_NEAR(box.min.y, 3.0f, 0.001f);
    EXPECT_NEAR(box.min.z, 4.5f, 0.001f);
    EXPECT_NEAR(box.max.x, 5.5f, 0.001f);
    EXPECT_NEAR(box.max.y, 5.0f, 0.001f);
    EXPECT_NEAR(box.max.z, 5.5f, 0.001f);
}

TEST_F(CharacterControllerTest, StepHeightAccessor) {
    EXPECT_NEAR(controller.stepHeight(), 1.0f, 0.001f);
    controller.setStepHeight(0.5f);
    EXPECT_NEAR(controller.stepHeight(), 0.5f, 0.001f);
}

TEST_F(CharacterControllerTest, NoDisplacementStaysInPlace) {
    fillFloor(0, -5, 5, -5, 5);
    Vec3f pos(0.0f, 1.0f, 0.0f);
    Vec3f move(0.0f, 0.0f, 0.0f);

    auto result = controller.move(pos, move, grid);
    EXPECT_TRUE(result.onGround);
    EXPECT_FALSE(result.hitX);
    EXPECT_FALSE(result.hitY);
    EXPECT_FALSE(result.hitZ);
    EXPECT_NEAR(result.resolvedPosition.x, 0.0f, 0.01f);
    EXPECT_NEAR(result.resolvedPosition.y, 1.0f, 0.01f);
}

TEST_F(CharacterControllerTest, DensityBelowThresholdIsPassable) {
    // Voxel with density below threshold should not block
    grid.set(3, 1, 0, 0.3f);
    grid.set(3, 2, 0, 0.3f);
    fillFloor(0, -5, 5, -5, 5);

    Vec3f pos(2.0f, 1.0f, 0.0f);
    Vec3f move(2.0f, 0.0f, 0.0f);

    auto result = controller.move(pos, move, grid);
    EXPECT_FALSE(result.hitX);
    EXPECT_NEAR(result.resolvedPosition.x, 4.0f, 0.01f);
}
