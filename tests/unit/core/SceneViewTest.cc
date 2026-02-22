#include "fabric/core/Camera.hh"
#include "fabric/core/Rendering.hh"
#include "fabric/core/Spatial.hh"
#include "fabric/utils/Testing.hh"
#include <gtest/gtest.h>
#include <bx/math.h>
#include <cmath>
#include <unordered_set>

using namespace fabric;

class FrustumCullerTest : public ::testing::Test {
protected:
    // Build a VP matrix using bx::mtxProj + bx::mtxLookAt for consistency
    // with Camera's internal implementation
    void buildVP(float* outVP, const float* view, const float* proj) {
        bx::mtxMul(outVP, view, proj);
    }
};

TEST_F(FrustumCullerTest, NodesWithoutAABBAlwaysVisible) {
    // Camera looking along +Z from origin
    Camera camera;
    camera.setPerspective(60.0f, 16.0f / 9.0f, 0.1f, 100.0f, true);
    Transform<float> camTransform;
    camera.updateView(camTransform);

    SceneNode root("root");
    root.createChild("child_a");
    root.createChild("child_b");

    float vp[16];
    camera.getViewProjection(vp);

    auto visible = FrustumCuller::cull(vp, root);

    // All 3 nodes (root + 2 children) should be visible since none have AABBs
    EXPECT_EQ(visible.size(), 3u);
}

TEST_F(FrustumCullerTest, NodeBehindCameraIsCulled) {
    // Camera at origin, looking along +Z (left-handed)
    Camera camera;
    camera.setPerspective(60.0f, 1.0f, 0.1f, 100.0f, true);
    Transform<float> camTransform;
    camera.updateView(camTransform);

    SceneNode root("root");
    auto* behindNode = root.createChild("behind");

    // Place AABB behind the camera (negative Z in left-handed = behind)
    AABB behindBox(
        Vec3f(-1.0f, -1.0f, -20.0f),
        Vec3f(1.0f, 1.0f, -10.0f)
    );
    behindNode->setAABB(behindBox);

    float vp[16];
    camera.getViewProjection(vp);

    auto visible = FrustumCuller::cull(vp, root);

    // Root has no AABB so it is visible; behind node should be culled
    bool foundBehind = false;
    for (SceneNode* n : visible) {
        if (n->getName() == "behind") foundBehind = true;
    }
    EXPECT_FALSE(foundBehind);
}

TEST_F(FrustumCullerTest, NodeInFrontOfCameraIsVisible) {
    Camera camera;
    camera.setPerspective(60.0f, 1.0f, 0.1f, 100.0f, true);
    Transform<float> camTransform;
    camera.updateView(camTransform);

    SceneNode root("root");
    auto* frontNode = root.createChild("front");

    // Place AABB in front of the camera (positive Z in left-handed)
    AABB frontBox(
        Vec3f(-1.0f, -1.0f, 5.0f),
        Vec3f(1.0f, 1.0f, 10.0f)
    );
    frontNode->setAABB(frontBox);

    float vp[16];
    camera.getViewProjection(vp);

    auto visible = FrustumCuller::cull(vp, root);

    bool foundFront = false;
    for (SceneNode* n : visible) {
        if (n->getName() == "front") foundFront = true;
    }
    EXPECT_TRUE(foundFront);
}

TEST_F(FrustumCullerTest, NodeFarOutsideFrustumIsCulled) {
    Camera camera;
    camera.setPerspective(60.0f, 1.0f, 0.1f, 100.0f, true);
    Transform<float> camTransform;
    camera.updateView(camTransform);

    SceneNode root("root");
    auto* farNode = root.createChild("far_right");

    // Place AABB far to the right, well outside 60-degree FOV
    AABB farBox(
        Vec3f(500.0f, 500.0f, 5.0f),
        Vec3f(510.0f, 510.0f, 10.0f)
    );
    farNode->setAABB(farBox);

    float vp[16];
    camera.getViewProjection(vp);

    auto visible = FrustumCuller::cull(vp, root);

    bool foundFar = false;
    for (SceneNode* n : visible) {
        if (n->getName() == "far_right") foundFar = true;
    }
    EXPECT_FALSE(foundFar);
}

TEST_F(FrustumCullerTest, SubtreeSkippedWhenParentCulled) {
    Camera camera;
    camera.setPerspective(60.0f, 1.0f, 0.1f, 100.0f, true);
    Transform<float> camTransform;
    camera.updateView(camTransform);

    SceneNode root("root");
    auto* outsideParent = root.createChild("outside_parent");

    // Parent has AABB outside frustum
    AABB outsideBox(
        Vec3f(500.0f, 500.0f, 5.0f),
        Vec3f(510.0f, 510.0f, 10.0f)
    );
    outsideParent->setAABB(outsideBox);

    // Child has no AABB (would normally be visible), but parent is culled
    outsideParent->createChild("child_under_culled");

    float vp[16];
    camera.getViewProjection(vp);

    auto visible = FrustumCuller::cull(vp, root);

    // Only root should be visible. The parent and its child are both culled.
    bool foundChild = false;
    bool foundParent = false;
    for (SceneNode* n : visible) {
        if (n->getName() == "child_under_culled") foundChild = true;
        if (n->getName() == "outside_parent") foundParent = true;
    }
    EXPECT_FALSE(foundParent);
    EXPECT_FALSE(foundChild);
}

TEST_F(FrustumCullerTest, MixedVisibility) {
    Camera camera;
    camera.setPerspective(60.0f, 1.0f, 0.1f, 100.0f, true);
    Transform<float> camTransform;
    camera.updateView(camTransform);

    SceneNode root("root");

    auto* visible1 = root.createChild("visible_1");
    visible1->setAABB(AABB(Vec3f(-1.0f, -1.0f, 5.0f), Vec3f(1.0f, 1.0f, 10.0f)));

    auto* culled1 = root.createChild("culled_1");
    culled1->setAABB(AABB(Vec3f(500.0f, 0.0f, 5.0f), Vec3f(510.0f, 1.0f, 10.0f)));

    auto* visible2 = root.createChild("visible_2");
    visible2->setAABB(AABB(Vec3f(-2.0f, -2.0f, 20.0f), Vec3f(2.0f, 2.0f, 25.0f)));

    auto* noAabb = root.createChild("no_aabb");

    float vp[16];
    camera.getViewProjection(vp);

    auto visibleNodes = FrustumCuller::cull(vp, root);

    // Count expected nodes: root (no aabb), visible_1, visible_2, no_aabb = 4
    // culled_1 should not appear
    std::unordered_set<std::string> names;
    for (SceneNode* n : visibleNodes) {
        names.insert(n->getName());
    }

    EXPECT_TRUE(names.count("root"));
    EXPECT_TRUE(names.count("visible_1"));
    EXPECT_TRUE(names.count("visible_2"));
    EXPECT_TRUE(names.count("no_aabb"));
    EXPECT_FALSE(names.count("culled_1"));
}

TEST_F(FrustumCullerTest, OrthoFrustumCull) {
    // Spatial.hh orthographic treats near/far as z-axis values directly.
    // Symmetric range ensures clip space works as expected.
    auto ortho = Matrix4x4<float>::orthographic(-10.0f, 10.0f, -10.0f, 10.0f, -10.0f, 10.0f);

    SceneNode root("root");
    auto* inside = root.createChild("inside");
    inside->setAABB(AABB(Vec3f(-1.0f, -1.0f, -1.0f), Vec3f(1.0f, 1.0f, 1.0f)));

    auto* outside = root.createChild("outside");
    outside->setAABB(AABB(Vec3f(20.0f, 20.0f, 1.0f), Vec3f(30.0f, 30.0f, 5.0f)));

    auto visible = FrustumCuller::cull(ortho.elements.data(), root);

    bool foundInside = false, foundOutside = false;
    for (SceneNode* n : visible) {
        if (n->getName() == "inside") foundInside = true;
        if (n->getName() == "outside") foundOutside = true;
    }

    EXPECT_TRUE(foundInside);
    EXPECT_FALSE(foundOutside);
}

// SceneNode AABB tests

TEST(SceneNodeAABBTest, DefaultHasNoAABB) {
    SceneNode node("test");
    EXPECT_EQ(node.getAABB(), nullptr);
}

TEST(SceneNodeAABBTest, SetAndGetAABB) {
    SceneNode node("test");
    AABB box(Vec3f(-1.0f, -1.0f, -1.0f), Vec3f(1.0f, 1.0f, 1.0f));
    node.setAABB(box);

    const AABB* result = node.getAABB();
    ASSERT_NE(result, nullptr);
    EXPECT_FLOAT_EQ(result->min.x, -1.0f);
    EXPECT_FLOAT_EQ(result->max.x, 1.0f);
}

TEST_F(FrustumCullerTest, CameraMovementChangesVisibleSet) {
    Camera camera;
    camera.setPerspective(60.0f, 1.0f, 0.1f, 100.0f, true);

    SceneNode root("root");
    auto* leftNode = root.createChild("left");
    leftNode->setAABB(AABB(Vec3f(-50.0f, -1.0f, 5.0f), Vec3f(-40.0f, 1.0f, 10.0f)));

    auto* rightNode = root.createChild("right");
    rightNode->setAABB(AABB(Vec3f(40.0f, -1.0f, 5.0f), Vec3f(50.0f, 1.0f, 10.0f)));

    // Camera at origin: neither far-left nor far-right should be visible
    Transform<float> camTransform;
    camera.updateView(camTransform);

    float vp[16];
    camera.getViewProjection(vp);
    auto visible1 = FrustumCuller::cull(vp, root);

    std::unordered_set<std::string> names1;
    for (SceneNode* n : visible1) names1.insert(n->getName());
    EXPECT_FALSE(names1.count("left"));
    EXPECT_FALSE(names1.count("right"));

    // Move camera far to the left: left node should now be visible
    camTransform.setPosition(Vec3f(-45.0f, 0.0f, 0.0f));
    camera.updateView(camTransform);
    camera.getViewProjection(vp);
    auto visible2 = FrustumCuller::cull(vp, root);

    std::unordered_set<std::string> names2;
    for (SceneNode* n : visible2) names2.insert(n->getName());
    EXPECT_TRUE(names2.count("left"));
    EXPECT_FALSE(names2.count("right"));
}

TEST_F(FrustumCullerTest, MultipleCamerasOnDifferentViews) {
    // Two cameras with different positions see different subsets of the scene
    Camera cameraA;
    cameraA.setPerspective(60.0f, 1.0f, 0.1f, 100.0f, true);

    Camera cameraB;
    cameraB.setPerspective(60.0f, 1.0f, 0.1f, 100.0f, true);

    SceneNode root("root");
    auto* nearCenter = root.createChild("near_center");
    nearCenter->setAABB(AABB(Vec3f(-1.0f, -1.0f, 5.0f), Vec3f(1.0f, 1.0f, 10.0f)));

    auto* farRight = root.createChild("far_right");
    farRight->setAABB(AABB(Vec3f(80.0f, -1.0f, 5.0f), Vec3f(90.0f, 1.0f, 10.0f)));

    // Camera A at origin: sees near_center, not far_right
    Transform<float> transformA;
    cameraA.updateView(transformA);

    float vpA[16];
    cameraA.getViewProjection(vpA);
    auto visibleA = FrustumCuller::cull(vpA, root);

    std::unordered_set<std::string> namesA;
    for (SceneNode* n : visibleA) namesA.insert(n->getName());
    EXPECT_TRUE(namesA.count("near_center"));
    EXPECT_FALSE(namesA.count("far_right"));

    // Camera B offset to the right: sees far_right, not near_center
    Transform<float> transformB;
    transformB.setPosition(Vec3f(85.0f, 0.0f, 0.0f));
    cameraB.updateView(transformB);

    float vpB[16];
    cameraB.getViewProjection(vpB);
    auto visibleB = FrustumCuller::cull(vpB, root);

    std::unordered_set<std::string> namesB;
    for (SceneNode* n : visibleB) namesB.insert(n->getName());
    EXPECT_FALSE(namesB.count("near_center"));
    EXPECT_TRUE(namesB.count("far_right"));
}
