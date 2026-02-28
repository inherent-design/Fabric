#include "fabric/core/ChunkedGrid.hh"
#include "fabric/core/PhysicsWorld.hh"

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>

#include <gtest/gtest.h>

using namespace fabric;

// Grid-based projectile raycast (DDA)

TEST(CCDTest, CastProjectileRayEmptyGrid) {
    PhysicsWorld pw;
    pw.init();

    ChunkedGrid<float> grid;

    // Ray through empty space should not hit
    auto hit = pw.castProjectileRay(grid, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 10.0f);
    EXPECT_FALSE(hit.has_value());

    pw.shutdown();
}

TEST(CCDTest, CastProjectileRayHitSolid) {
    PhysicsWorld pw;
    pw.init();

    ChunkedGrid<float> grid;
    grid.set(5, 0, 0, 1.0f);

    // Ray from origin towards x=5 should hit
    auto hit = pw.castProjectileRay(grid, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 10.0f);
    ASSERT_TRUE(hit.has_value());
    EXPECT_EQ(hit->x, 5);
    EXPECT_EQ(hit->y, 0);
    EXPECT_EQ(hit->z, 0);
    EXPECT_EQ(hit->nx, -1);
    EXPECT_NEAR(hit->t, 5.0f, 0.01f);

    pw.shutdown();
}

TEST(CCDTest, CastProjectileRayCustomThreshold) {
    PhysicsWorld pw;
    pw.init();

    ChunkedGrid<float> grid;
    grid.set(5, 0, 0, 0.3f);
    grid.set(10, 0, 0, 0.8f);

    // Threshold 0.5: only density 0.8 hits
    auto hit1 = pw.castProjectileRay(grid, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 15.0f, 0.5f);
    ASSERT_TRUE(hit1.has_value());
    EXPECT_EQ(hit1->x, 10);

    // Threshold 0.2: both densities hit (first one at x=5)
    auto hit2 = pw.castProjectileRay(grid, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 15.0f, 0.2f);
    ASSERT_TRUE(hit2.has_value());
    EXPECT_EQ(hit2->x, 5);

    pw.shutdown();
}

TEST(CCDTest, CastProjectileRayMaxDistance) {
    PhysicsWorld pw;
    pw.init();

    ChunkedGrid<float> grid;
    grid.set(100, 0, 0, 1.0f);

    // Ray with max distance 50 should not hit voxel at x=100
    auto hit = pw.castProjectileRay(grid, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 50.0f);
    EXPECT_FALSE(hit.has_value());

    pw.shutdown();
}

TEST(CCDTest, CastProjectileRayDiagonal) {
    PhysicsWorld pw;
    pw.init();

    ChunkedGrid<float> grid;
    grid.set(5, 5, 0, 1.0f);

    // Diagonal ray should hit
    auto hit = pw.castProjectileRay(grid, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 10.0f);
    ASSERT_TRUE(hit.has_value());
    EXPECT_EQ(hit->x, 5);
    EXPECT_EQ(hit->y, 5);

    pw.shutdown();
}

TEST(CCDTest, CastProjectileRayNegativeDirection) {
    PhysicsWorld pw;
    pw.init();

    ChunkedGrid<float> grid;
    grid.set(-5, 0, 0, 1.0f);

    // Ray in negative x direction
    auto hit = pw.castProjectileRay(grid, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, 10.0f);
    ASSERT_TRUE(hit.has_value());
    EXPECT_EQ(hit->x, -5);
    EXPECT_EQ(hit->nx, 1);

    pw.shutdown();
}

TEST(CCDTest, CastProjectileRayStartingInSolid) {
    PhysicsWorld pw;
    pw.init();

    ChunkedGrid<float> grid;
    grid.set(0, 0, 0, 1.0f);

    // Ray starting inside solid should report hit at origin
    auto hit = pw.castProjectileRay(grid, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 10.0f);
    ASSERT_TRUE(hit.has_value());
    EXPECT_EQ(hit->x, 0);
    EXPECT_EQ(hit->t, 0.0f);

    pw.shutdown();
}

TEST(CCDTest, CastProjectileRayBeforeInit) {
    PhysicsWorld pw;
    // Not calling init()

    ChunkedGrid<float> grid;
    grid.set(5, 0, 0, 1.0f);

    // Should still work (doesn't depend on Jolt initialization)
    auto hit = pw.castProjectileRay(grid, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 10.0f);
    EXPECT_TRUE(hit.has_value());
}

// Swept AABB intersection

TEST(CCDTest, SweptAABBNoMotion) {
    PhysicsWorld pw;
    pw.init();

    // Static overlap
    bool hit = pw.sweptAABBIntersect(0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.016f, 0.5f, 0.5f, 0.5f,
                                     1.5f, 1.5f, 1.5f);
    EXPECT_TRUE(hit);

    pw.shutdown();
}

TEST(CCDTest, SweptAABBMovingToward) {
    PhysicsWorld pw;
    pw.init();

    float t;
    // AABB at origin moving toward static AABB at (2,0,0)
    bool hit = pw.sweptAABBIntersect(0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 20.0f, 0.0f, 0.0f, 0.1f, 2.0f, 0.0f, 0.0f,
                                     3.0f, 1.0f, 1.0f, &t);
    EXPECT_TRUE(hit);
    EXPECT_NEAR(t, 0.05f, 0.005f);

    pw.shutdown();
}

TEST(CCDTest, SweptAABBMovingAway) {
    PhysicsWorld pw;
    pw.init();

    // AABB at origin moving away from AABB at (2,0,0)
    bool hit = pw.sweptAABBIntersect(0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, -1.0f, 0.0f, 0.0f, 0.1f, 2.0f, 0.0f, 0.0f,
                                     3.0f, 1.0f, 1.0f);
    EXPECT_FALSE(hit);

    pw.shutdown();
}

TEST(CCDTest, SweptAABBDiagonalMotion) {
    PhysicsWorld pw;
    pw.init();

    float t;
    // Moving diagonally toward target
    bool hit = pw.sweptAABBIntersect(0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 10.0f, 10.0f, 10.0f, 0.2f, 2.0f, 2.0f, 2.0f,
                                     3.0f, 3.0f, 3.0f, &t);
    EXPECT_TRUE(hit);
    EXPECT_NEAR(t, 0.1f, 0.005f);

    pw.shutdown();
}

TEST(CCDTest, SweptAABBNegativeCoordinates) {
    PhysicsWorld pw;
    pw.init();

    // Negative space collision
    bool hit = pw.sweptAABBIntersect(-5.0f, -5.0f, -5.0f, -4.0f, -4.0f, -4.0f, 200.0f, 0.0f, 0.0f, 0.01f, -3.0f, -5.0f,
                                     -5.0f, -2.0f, -4.0f, -4.0f);
    EXPECT_TRUE(hit);

    pw.shutdown();
}

TEST(CCDTest, SweptAABBReversedMinMax) {
    PhysicsWorld pw;
    pw.init();

    // AABB with reversed min/max should be handled internally
    bool hit = pw.sweptAABBIntersect(1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.01f, 1.5f, 1.5f, 1.5f,
                                     0.5f, 0.5f, 0.5f);
    EXPECT_TRUE(hit);

    pw.shutdown();
}

TEST(CCDTest, SweptAABBPartialOverlap) {
    PhysicsWorld pw;
    pw.init();

    float t;
    // Moving AABB starts partially overlapping static AABB
    bool hit = pw.sweptAABBIntersect(0.0f, 0.0f, 0.0f, 1.0f, 2.0f, 1.0f, 0.5f, 0.0f, 0.0f, 0.1f, 0.5f, 1.0f, 0.0f, 1.5f,
                                     3.0f, 1.0f, &t);
    EXPECT_TRUE(hit);
    EXPECT_NEAR(t, 0.0f, 0.001f);

    pw.shutdown();
}

TEST(CCDTest, SweptAABBNoHitBeyondDT) {
    PhysicsWorld pw;
    pw.init();

    float t;
    // Motion would hit after dt, so should not report collision
    bool hit = pw.sweptAABBIntersect(0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0.1f, 0.0f, 0.0f, 0.05f, 10.0f, 0.0f, 0.0f,
                                     11.0f, 1.0f, 1.0f, &t);
    EXPECT_FALSE(hit);

    pw.shutdown();
}

TEST(CCDTest, SweptAABBZeroVelocity) {
    PhysicsWorld pw;
    pw.init();

    // Zero velocity with overlapping AABBs
    bool hit = pw.sweptAABBIntersect(0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.5f, 0.5f, 0.5f, 1.5f,
                                     1.5f, 1.5f);
    EXPECT_TRUE(hit);

    // Zero velocity with non-overlapping AABBs
    hit = pw.sweptAABBIntersect(0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 2.0f, 0.0f, 0.0f, 3.0f,
                                1.0f, 1.0f);
    EXPECT_FALSE(hit);

    pw.shutdown();
}
