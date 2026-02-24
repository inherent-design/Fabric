#include "fabric/core/VoxelRaycast.hh"
#include <gtest/gtest.h>

using namespace fabric;

class VoxelRaycastTest : public ::testing::Test {
  protected:
    ChunkedGrid<float> grid;
};

TEST_F(VoxelRaycastTest, RayHitsSingleVoxel) {
    grid.set(5, 5, 5, 1.0f);
    auto hit = castRay(grid, 5.5f, 5.5f, 0.5f, 0.0f, 0.0f, 1.0f);
    ASSERT_TRUE(hit.has_value());
    EXPECT_EQ(hit->x, 5);
    EXPECT_EQ(hit->y, 5);
    EXPECT_EQ(hit->z, 5);
    EXPECT_EQ(hit->nx, 0);
    EXPECT_EQ(hit->ny, 0);
    EXPECT_EQ(hit->nz, -1);
}

TEST_F(VoxelRaycastTest, RayMissesEmptyGrid) {
    auto hit = castRay(grid, 0.5f, 0.5f, 0.5f, 1.0f, 0.0f, 0.0f);
    EXPECT_FALSE(hit.has_value());
}

TEST_F(VoxelRaycastTest, RayMaxDistanceRespected) {
    grid.set(100, 0, 0, 1.0f);
    auto hit = castRay(grid, 0.5f, 0.5f, 0.5f, 1.0f, 0.0f, 0.0f, 50.0f);
    EXPECT_FALSE(hit.has_value());
}

TEST_F(VoxelRaycastTest, RayHitsNearestFace) {
    grid.set(5, 5, 3, 1.0f);
    grid.set(5, 5, 5, 1.0f);
    grid.set(5, 5, 7, 1.0f);
    auto hit = castRay(grid, 5.5f, 5.5f, 0.5f, 0.0f, 0.0f, 1.0f);
    ASSERT_TRUE(hit.has_value());
    EXPECT_EQ(hit->z, 3);
}

TEST_F(VoxelRaycastTest, RayAtAngle) {
    grid.set(3, 3, 3, 1.0f);
    float invSqrt3 = 1.0f / std::sqrt(3.0f);
    auto hit = castRay(grid, 0.5f, 0.5f, 0.5f, invSqrt3, invSqrt3, invSqrt3);
    ASSERT_TRUE(hit.has_value());
    EXPECT_EQ(hit->x, 3);
    EXPECT_EQ(hit->y, 3);
    EXPECT_EQ(hit->z, 3);
}

TEST_F(VoxelRaycastTest, RayNegativeCoordinates) {
    grid.set(-5, -3, -7, 1.0f);
    auto hit = castRay(grid, -4.5f, -2.5f, 0.5f, 0.0f, 0.0f, -1.0f);
    ASSERT_TRUE(hit.has_value());
    EXPECT_EQ(hit->x, -5);
    EXPECT_EQ(hit->y, -3);
    EXPECT_EQ(hit->z, -7);
}

TEST_F(VoxelRaycastTest, CastRayAllReturnsMultipleHits) {
    grid.set(5, 5, 3, 1.0f);
    grid.set(5, 5, 6, 1.0f);
    grid.set(5, 5, 9, 1.0f);
    auto hits = castRayAll(grid, 5.5f, 5.5f, 0.5f, 0.0f, 0.0f, 1.0f, 256.0f);
    ASSERT_EQ(hits.size(), 3u);
    EXPECT_EQ(hits[0].z, 3);
    EXPECT_EQ(hits[1].z, 6);
    EXPECT_EQ(hits[2].z, 9);
}

TEST_F(VoxelRaycastTest, RayOriginInsideSolid) {
    grid.set(5, 5, 5, 1.0f);
    auto hit = castRay(grid, 5.5f, 5.5f, 5.5f, 1.0f, 0.0f, 0.0f);
    ASSERT_TRUE(hit.has_value());
    EXPECT_EQ(hit->x, 5);
    EXPECT_EQ(hit->y, 5);
    EXPECT_EQ(hit->z, 5);
    EXPECT_NEAR(hit->t, 0.0f, 0.001f);
}
