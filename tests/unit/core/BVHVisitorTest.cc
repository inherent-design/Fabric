#include "fabric/utils/BVH.hh"

#include <gtest/gtest.h>

using namespace fabric;

static AABB makeBox(float x, float y, float z, float half) {
    return AABB(Vec3f(x - half, y - half, z - half), Vec3f(x + half, y + half, z + half));
}

TEST(BVHVisitorTest, EmptyBVHProducesNoVisits) {
    BVH<int> bvh;
    bvh.build();

    int count = 0;
    bvh.visitNodes([&](const AABB&, int, bool) { ++count; });
    EXPECT_EQ(count, 0);
}

TEST(BVHVisitorTest, SingleItemVisitsOneLeaf) {
    BVH<int> bvh;
    bvh.insert(makeBox(0, 0, 0, 1), 42);
    bvh.build();

    int count = 0;
    bool sawLeaf = false;
    bvh.visitNodes([&](const AABB&, int depth, bool isLeaf) {
        ++count;
        if (isLeaf)
            sawLeaf = true;
        EXPECT_EQ(depth, 0);
    });
    EXPECT_EQ(count, 1);
    EXPECT_TRUE(sawLeaf);
}

TEST(BVHVisitorTest, VisitsAllNodes) {
    BVH<int> bvh;
    for (int i = 0; i < 4; ++i) {
        bvh.insert(makeBox(static_cast<float>(i * 10), 0, 0, 1), i);
    }
    bvh.build();

    int totalNodes = 0;
    int leafCount = 0;
    int internalCount = 0;
    bvh.visitNodes([&](const AABB&, int, bool isLeaf) {
        ++totalNodes;
        if (isLeaf)
            ++leafCount;
        else
            ++internalCount;
    });

    EXPECT_EQ(leafCount, 4);
    EXPECT_EQ(internalCount, 3);
    EXPECT_EQ(totalNodes, 7);
}

TEST(BVHVisitorTest, DepthIncreases) {
    BVH<int> bvh;
    for (int i = 0; i < 8; ++i) {
        bvh.insert(makeBox(static_cast<float>(i * 10), 0, 0, 1), i);
    }
    bvh.build();

    int maxDepth = 0;
    bool rootAtZero = false;
    bvh.visitNodes([&](const AABB&, int depth, bool) {
        if (depth == 0)
            rootAtZero = true;
        if (depth > maxDepth)
            maxDepth = depth;
    });

    EXPECT_TRUE(rootAtZero);
    EXPECT_GT(maxDepth, 0);
}

TEST(BVHVisitorTest, LeavesAtGreaterOrEqualDepthThanInternal) {
    BVH<int> bvh;
    for (int i = 0; i < 4; ++i) {
        bvh.insert(makeBox(static_cast<float>(i * 10), 0, 0, 1), i);
    }
    bvh.build();

    int minLeafDepth = 999;
    int maxInternalDepth = -1;
    bvh.visitNodes([&](const AABB&, int depth, bool isLeaf) {
        if (isLeaf && depth < minLeafDepth)
            minLeafDepth = depth;
        if (!isLeaf && depth > maxInternalDepth)
            maxInternalDepth = depth;
    });

    EXPECT_GE(minLeafDepth, maxInternalDepth);
}

TEST(BVHVisitorTest, AutoBuildsWhenDirty) {
    BVH<int> bvh;
    bvh.insert(makeBox(0, 0, 0, 1), 1);
    bvh.insert(makeBox(10, 0, 0, 1), 2);

    int count = 0;
    bvh.visitNodes([&](const AABB&, int, bool) { ++count; });
    EXPECT_EQ(count, 3); // 2 leaves + 1 internal
}

TEST(BVHVisitorTest, WorksOnConstRef) {
    BVH<int> bvh;
    bvh.insert(makeBox(0, 0, 0, 1), 1);
    bvh.build();

    const auto& constRef = bvh;
    int count = 0;
    constRef.visitNodes([&](const AABB&, int, bool) { ++count; });
    EXPECT_EQ(count, 1);
}
