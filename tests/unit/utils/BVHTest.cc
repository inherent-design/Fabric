#include "fabric/utils/BVH.hh"
#include <chrono>
#include <gtest/gtest.h>
#include <set>

using namespace fabric;

static AABB makeBox(float x, float y, float z, float half) {
    return AABB(Vec3f(x - half, y - half, z - half), Vec3f(x + half, y + half, z + half));
}

TEST(BVHTest, EmptyBVH) {
    BVH<int> bvh;
    EXPECT_TRUE(bvh.empty());
    EXPECT_EQ(bvh.size(), 0u);

    auto results = bvh.query(makeBox(0, 0, 0, 10));
    EXPECT_TRUE(results.empty());
}

TEST(BVHTest, SingleInsertQueryHit) {
    BVH<int> bvh;
    bvh.insert(makeBox(5, 5, 5, 1), 42);

    auto results = bvh.query(makeBox(5, 5, 5, 2));
    ASSERT_EQ(results.size(), 1u);
    EXPECT_EQ(results[0], 42);
}

TEST(BVHTest, SingleInsertQueryMiss) {
    BVH<int> bvh;
    bvh.insert(makeBox(5, 5, 5, 1), 42);

    auto results = bvh.query(makeBox(100, 100, 100, 1));
    EXPECT_TRUE(results.empty());
}

TEST(BVHTest, TenItemsQueryThree) {
    BVH<int> bvh;
    // Place 10 items along x-axis at x = 0, 10, 20, ..., 90
    for (int i = 0; i < 10; ++i) {
        bvh.insert(makeBox(static_cast<float>(i * 10), 0, 0, 1), i);
    }

    // Query region overlapping items at x=0, x=10, x=20 (ids 0, 1, 2)
    AABB region(Vec3f(-2, -2, -2), Vec3f(22, 2, 2));
    auto results = bvh.query(region);

    std::set<int> found(results.begin(), results.end());
    EXPECT_EQ(found.size(), 3u);
    EXPECT_TRUE(found.count(0));
    EXPECT_TRUE(found.count(1));
    EXPECT_TRUE(found.count(2));
}

TEST(BVHTest, RemoveItem) {
    BVH<int> bvh;
    bvh.insert(makeBox(0, 0, 0, 1), 1);
    bvh.insert(makeBox(10, 0, 0, 1), 2);
    bvh.insert(makeBox(20, 0, 0, 1), 3);

    EXPECT_TRUE(bvh.remove(2));
    EXPECT_EQ(bvh.size(), 2u);

    // Query the whole space
    auto results = bvh.query(makeBox(10, 0, 0, 50));
    std::set<int> found(results.begin(), results.end());
    EXPECT_FALSE(found.count(2));
    EXPECT_TRUE(found.count(1));
    EXPECT_TRUE(found.count(3));
}

TEST(BVHTest, RemoveNonExistent) {
    BVH<int> bvh;
    bvh.insert(makeBox(0, 0, 0, 1), 1);
    EXPECT_FALSE(bvh.remove(999));
    EXPECT_EQ(bvh.size(), 1u);
}

TEST(BVHTest, BuildQueryConsistency) {
    BVH<int> bvh;
    for (int i = 0; i < 5; ++i) {
        bvh.insert(makeBox(static_cast<float>(i * 10), 0, 0, 1), i);
    }

    // Query before explicit build (auto-builds)
    AABB region(Vec3f(-2, -2, -2), Vec3f(12, 2, 2));
    auto before = bvh.query(region);

    // Explicit build then query again
    bvh.build();
    auto after = bvh.query(region);

    std::set<int> setBefore(before.begin(), before.end());
    std::set<int> setAfter(after.begin(), after.end());
    EXPECT_EQ(setBefore, setAfter);
}

TEST(BVHTest, FrustumQuery) {
    BVH<int> bvh;
    // Items in front of camera (negative z in view space with lookAt down -z)
    bvh.insert(makeBox(0, 0, -5, 1), 1);  // visible
    bvh.insert(makeBox(0, 0, -15, 1), 2); // visible
    bvh.insert(makeBox(0, 0, 50, 1), 3);  // behind camera

    // Build a view-projection matrix: camera at origin looking down -z
    auto view = Matrix4x4<float>::lookAt(Vec3f(0, 0, 0), Vec3f(0, 0, -1), Vec3f(0, 1, 0));
    auto proj = Matrix4x4<float>::perspective(1.5708f, // ~90 degrees FOV
                                              1.0f,    // aspect ratio
                                              0.1f,    // near
                                              100.0f   // far
    );
    auto vp = proj * view;

    Frustum frustum;
    frustum.extractFromVP(vp.elements.data());

    auto results = bvh.queryFrustum(frustum);
    std::set<int> found(results.begin(), results.end());

    EXPECT_TRUE(found.count(1));
    EXPECT_TRUE(found.count(2));
    EXPECT_FALSE(found.count(3));
}

TEST(BVHTest, LargeBatchPerformance) {
    BVH<int> bvh;
    // Insert 1000 items in a 100x10x1 grid
    for (int i = 0; i < 1000; ++i) {
        float x = static_cast<float>(i % 100) * 2.0f;
        float y = static_cast<float>((i / 100) % 10) * 2.0f;
        float z = 0.0f;
        bvh.insert(makeBox(x, y, z, 0.5f), i);
    }

    bvh.build();

    // Query a small region that should hit ~10 items
    AABB region(Vec3f(-1, -1, -1), Vec3f(9, 1, 1));

    auto start = std::chrono::high_resolution_clock::now();
    auto results = bvh.query(region);
    auto elapsed = std::chrono::high_resolution_clock::now() - start;

    EXPECT_FALSE(results.empty());
    EXPECT_LT(results.size(), 100u); // sanity: not returning everything

    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
    EXPECT_LT(ms, 10);
}

TEST(BVHTest, ClearResetsState) {
    BVH<int> bvh;
    bvh.insert(makeBox(0, 0, 0, 1), 1);
    bvh.insert(makeBox(10, 0, 0, 1), 2);
    EXPECT_EQ(bvh.size(), 2u);

    bvh.clear();
    EXPECT_EQ(bvh.size(), 0u);
    EXPECT_TRUE(bvh.empty());

    auto results = bvh.query(makeBox(0, 0, 0, 100));
    EXPECT_TRUE(results.empty());
}
