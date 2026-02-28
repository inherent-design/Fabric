#include "fabric/core/Rendering.hh"
#include "fabric/utils/Testing.hh"
#include <cmath>
#include <gtest/gtest.h>

using namespace fabric;

template <typename T> bool almostEq(T a, T b, T epsilon = static_cast<T>(1e-5)) {
    return std::abs(a - b) <= epsilon;
}

class RenderingTest : public ::testing::Test {
  protected:
    void SetUp() override {}
};

// AABB tests

TEST_F(RenderingTest, AABBDefaultConstruction) {
    AABB aabb;
    EXPECT_FLOAT_EQ(aabb.min.x, 0.0f);
    EXPECT_FLOAT_EQ(aabb.min.y, 0.0f);
    EXPECT_FLOAT_EQ(aabb.min.z, 0.0f);
    EXPECT_FLOAT_EQ(aabb.max.x, 0.0f);
    EXPECT_FLOAT_EQ(aabb.max.y, 0.0f);
    EXPECT_FLOAT_EQ(aabb.max.z, 0.0f);
}

TEST_F(RenderingTest, AABBConstructionWithMinMax) {
    Vec3f lo(-1.0f, -2.0f, -3.0f);
    Vec3f hi(4.0f, 5.0f, 6.0f);
    AABB aabb(lo, hi);

    EXPECT_FLOAT_EQ(aabb.min.x, -1.0f);
    EXPECT_FLOAT_EQ(aabb.max.x, 4.0f);

    Vec3f c = aabb.center();
    EXPECT_FLOAT_EQ(c.x, 1.5f);
    EXPECT_FLOAT_EQ(c.y, 1.5f);
    EXPECT_FLOAT_EQ(c.z, 1.5f);

    Vec3f e = aabb.extents();
    EXPECT_FLOAT_EQ(e.x, 2.5f);
    EXPECT_FLOAT_EQ(e.y, 3.5f);
    EXPECT_FLOAT_EQ(e.z, 4.5f);
}

TEST_F(RenderingTest, AABBContainsPoint) {
    AABB aabb(Vec3f(-1.0f, -1.0f, -1.0f), Vec3f(1.0f, 1.0f, 1.0f));

    EXPECT_TRUE(aabb.contains(Vec3f(0.0f, 0.0f, 0.0f)));
    EXPECT_TRUE(aabb.contains(Vec3f(1.0f, 1.0f, 1.0f)));
    EXPECT_FALSE(aabb.contains(Vec3f(2.0f, 0.0f, 0.0f)));
    EXPECT_FALSE(aabb.contains(Vec3f(0.0f, -2.0f, 0.0f)));
}

TEST_F(RenderingTest, AABBExpandByPoint) {
    AABB aabb(Vec3f(0.0f, 0.0f, 0.0f), Vec3f(1.0f, 1.0f, 1.0f));

    aabb.expand(Vec3f(3.0f, -1.0f, 2.0f));
    EXPECT_FLOAT_EQ(aabb.max.x, 3.0f);
    EXPECT_FLOAT_EQ(aabb.min.y, -1.0f);
    EXPECT_FLOAT_EQ(aabb.max.z, 2.0f);
}

TEST_F(RenderingTest, AABBIntersects) {
    AABB a(Vec3f(0.0f, 0.0f, 0.0f), Vec3f(2.0f, 2.0f, 2.0f));
    AABB b(Vec3f(1.0f, 1.0f, 1.0f), Vec3f(3.0f, 3.0f, 3.0f));
    AABB c(Vec3f(5.0f, 5.0f, 5.0f), Vec3f(6.0f, 6.0f, 6.0f));

    EXPECT_TRUE(a.intersects(b));
    EXPECT_TRUE(b.intersects(a));
    EXPECT_FALSE(a.intersects(c));
    EXPECT_FALSE(c.intersects(a));
}

// Frustum tests

TEST_F(RenderingTest, FrustumExtractAndTestAABB) {
    // Build an orthographic VP matrix that maps [-10,10] in x,y,z to clip space
    auto ortho = Matrix4x4<float>::orthographic(-10.0f, 10.0f, -10.0f, 10.0f, -10.0f, 10.0f);

    Frustum frustum;
    frustum.extractFromVP(ortho.elements.data());

    // Box fully inside
    AABB inside(Vec3f(-1.0f, -1.0f, -1.0f), Vec3f(1.0f, 1.0f, 1.0f));
    EXPECT_NE(frustum.testAABB(inside), CullResult::Outside);

    // Box fully outside
    AABB outside(Vec3f(20.0f, 20.0f, 20.0f), Vec3f(30.0f, 30.0f, 30.0f));
    EXPECT_EQ(frustum.testAABB(outside), CullResult::Outside);

    // Box intersecting the frustum boundary
    AABB intersecting(Vec3f(9.0f, -1.0f, -1.0f), Vec3f(15.0f, 1.0f, 1.0f));
    auto result = frustum.testAABB(intersecting);
    EXPECT_NE(result, CullResult::Outside);
}

// RenderList tests

TEST_F(RenderingTest, RenderListEmpty) {
    RenderList list;
    EXPECT_TRUE(list.empty());
    EXPECT_EQ(list.size(), 0u);
}

TEST_F(RenderingTest, RenderListAddAndSort) {
    RenderList list;

    DrawCall c1;
    c1.sortKey = 30;
    DrawCall c2;
    c2.sortKey = 10;
    DrawCall c3;
    c3.sortKey = 20;

    list.addDrawCall(c1);
    list.addDrawCall(c2);
    list.addDrawCall(c3);

    EXPECT_EQ(list.size(), 3u);
    EXPECT_FALSE(list.empty());

    list.sortByKey();
    const auto& calls = list.drawCalls();
    EXPECT_EQ(calls[0].sortKey, 10u);
    EXPECT_EQ(calls[1].sortKey, 20u);
    EXPECT_EQ(calls[2].sortKey, 30u);
}

TEST_F(RenderingTest, RenderListClear) {
    RenderList list;
    DrawCall c;
    c.sortKey = 1;
    list.addDrawCall(c);
    EXPECT_EQ(list.size(), 1u);

    list.clear();
    EXPECT_TRUE(list.empty());
    EXPECT_EQ(list.size(), 0u);
}

// TransformInterpolator tests

TEST_F(RenderingTest, TransformInterpolateAlpha0) {
    Transform<float> prev;
    prev.setPosition(Vector3<float, Space::World>(1.0f, 2.0f, 3.0f));
    prev.setScale(Vector3<float, Space::World>(1.0f, 1.0f, 1.0f));

    Transform<float> current;
    current.setPosition(Vector3<float, Space::World>(5.0f, 6.0f, 7.0f));
    current.setScale(Vector3<float, Space::World>(2.0f, 2.0f, 2.0f));

    auto result = TransformInterpolator::interpolate(prev, current, 0.0f);

    EXPECT_FLOAT_EQ(result.getPosition().x, 1.0f);
    EXPECT_FLOAT_EQ(result.getPosition().y, 2.0f);
    EXPECT_FLOAT_EQ(result.getPosition().z, 3.0f);
    EXPECT_FLOAT_EQ(result.getScale().x, 1.0f);
}

TEST_F(RenderingTest, TransformInterpolateAlpha1) {
    Transform<float> prev;
    prev.setPosition(Vector3<float, Space::World>(1.0f, 2.0f, 3.0f));

    Transform<float> current;
    current.setPosition(Vector3<float, Space::World>(5.0f, 6.0f, 7.0f));

    auto result = TransformInterpolator::interpolate(prev, current, 1.0f);

    EXPECT_FLOAT_EQ(result.getPosition().x, 5.0f);
    EXPECT_FLOAT_EQ(result.getPosition().y, 6.0f);
    EXPECT_FLOAT_EQ(result.getPosition().z, 7.0f);
}

TEST_F(RenderingTest, TransformInterpolateMidpoint) {
    Transform<float> prev;
    prev.setPosition(Vector3<float, Space::World>(0.0f, 0.0f, 0.0f));
    prev.setScale(Vector3<float, Space::World>(1.0f, 1.0f, 1.0f));

    Transform<float> current;
    current.setPosition(Vector3<float, Space::World>(10.0f, 20.0f, 30.0f));
    current.setScale(Vector3<float, Space::World>(3.0f, 3.0f, 3.0f));

    auto result = TransformInterpolator::interpolate(prev, current, 0.5f);

    EXPECT_FLOAT_EQ(result.getPosition().x, 5.0f);
    EXPECT_FLOAT_EQ(result.getPosition().y, 10.0f);
    EXPECT_FLOAT_EQ(result.getPosition().z, 15.0f);
    EXPECT_FLOAT_EQ(result.getScale().x, 2.0f);
    EXPECT_FLOAT_EQ(result.getScale().y, 2.0f);
    EXPECT_FLOAT_EQ(result.getScale().z, 2.0f);
}
