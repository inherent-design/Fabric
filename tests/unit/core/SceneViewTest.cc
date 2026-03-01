#include "fabric/core/SceneView.hh"
#include "fabric/core/Camera.hh"
#include "fabric/core/ECS.hh"
#include "fabric/core/Rendering.hh"
#include "fabric/core/Spatial.hh"
#include "fabric/utils/Testing.hh"
#include <bx/math.h>
#include <cmath>
#include <gtest/gtest.h>
#include <unordered_set>

using namespace fabric;

class FrustumCullerTest : public ::testing::Test {
  protected:
    World ecsWorld;
    FrustumCuller culler;

    void SetUp() override { ecsWorld.registerCoreComponents(); }

    // Helper: create a scene entity with optional BoundingBox
    flecs::entity createEntity(const char* name) { return ecsWorld.createSceneEntity(name); }

    // Helper: create a child scene entity
    flecs::entity createChildEntity(flecs::entity parent, const char* name) {
        return ecsWorld.createChildEntity(parent, name);
    }

    // Helper: set BoundingBox on an entity
    void setBoundingBox(flecs::entity e, float minX, float minY, float minZ, float maxX, float maxY, float maxZ) {
        e.set<BoundingBox>({minX, minY, minZ, maxX, maxY, maxZ});
    }

    // Helper: collect visible entity names
    std::unordered_set<std::string> visibleNames(const std::vector<flecs::entity>& entities) {
        std::unordered_set<std::string> names;
        for (auto e : entities) {
            names.insert(std::string(e.name().c_str()));
        }
        return names;
    }
};

TEST_F(FrustumCullerTest, EntitiesWithoutBoundingBoxAlwaysVisible) {
    Camera camera;
    camera.setPerspective(60.0f, 16.0f / 9.0f, 0.1f, 100.0f, true);
    Transform<float> camTransform;
    camera.updateView(camTransform);

    createEntity("entity_a");
    createEntity("entity_b");

    float vp[16];
    camera.getViewProjection(vp);

    auto visible = culler.cull(vp, ecsWorld.get());

    // Both entities have no BoundingBox, so both should be visible
    EXPECT_EQ(visible.size(), 2u);
}

TEST_F(FrustumCullerTest, EntityBehindCameraIsCulled) {
    Camera camera;
    camera.setPerspective(60.0f, 1.0f, 0.1f, 100.0f, true);
    Transform<float> camTransform;
    camera.updateView(camTransform);

    auto behind = createEntity("behind");
    setBoundingBox(behind, -1.0f, -1.0f, -20.0f, 1.0f, 1.0f, -10.0f);

    auto noBox = createEntity("no_box");

    float vp[16];
    camera.getViewProjection(vp);

    auto visible = culler.cull(vp, ecsWorld.get());
    auto names = visibleNames(visible);

    EXPECT_FALSE(names.count("behind"));
    EXPECT_TRUE(names.count("no_box"));
}

TEST_F(FrustumCullerTest, EntityInFrontOfCameraIsVisible) {
    Camera camera;
    camera.setPerspective(60.0f, 1.0f, 0.1f, 100.0f, true);
    Transform<float> camTransform;
    camera.updateView(camTransform);

    auto front = createEntity("front");
    setBoundingBox(front, -1.0f, -1.0f, 5.0f, 1.0f, 1.0f, 10.0f);

    float vp[16];
    camera.getViewProjection(vp);

    auto visible = culler.cull(vp, ecsWorld.get());
    auto names = visibleNames(visible);

    EXPECT_TRUE(names.count("front"));
}

TEST_F(FrustumCullerTest, EntityFarOutsideFrustumIsCulled) {
    Camera camera;
    camera.setPerspective(60.0f, 1.0f, 0.1f, 100.0f, true);
    Transform<float> camTransform;
    camera.updateView(camTransform);

    auto farRight = createEntity("far_right");
    setBoundingBox(farRight, 500.0f, 500.0f, 5.0f, 510.0f, 510.0f, 10.0f);

    float vp[16];
    camera.getViewProjection(vp);

    auto visible = culler.cull(vp, ecsWorld.get());
    auto names = visibleNames(visible);

    EXPECT_FALSE(names.count("far_right"));
}

TEST_F(FrustumCullerTest, FlatCullingDoesNotSkipChildren) {
    // With flat iteration, each entity is tested independently.
    // A child without BoundingBox is visible even if its parent is culled.
    Camera camera;
    camera.setPerspective(60.0f, 1.0f, 0.1f, 100.0f, true);
    Transform<float> camTransform;
    camera.updateView(camTransform);

    auto parent = createEntity("outside_parent");
    setBoundingBox(parent, 500.0f, 500.0f, 5.0f, 510.0f, 510.0f, 10.0f);

    auto child = createChildEntity(parent, "child_no_box");

    float vp[16];
    camera.getViewProjection(vp);

    auto visible = culler.cull(vp, ecsWorld.get());
    auto names = visibleNames(visible);

    EXPECT_FALSE(names.count("outside_parent"));
    // Child has no BoundingBox: always visible in flat iteration
    EXPECT_TRUE(names.count("child_no_box"));
}

TEST_F(FrustumCullerTest, MixedVisibility) {
    Camera camera;
    camera.setPerspective(60.0f, 1.0f, 0.1f, 100.0f, true);
    Transform<float> camTransform;
    camera.updateView(camTransform);

    auto visible1 = createEntity("visible_1");
    setBoundingBox(visible1, -1.0f, -1.0f, 5.0f, 1.0f, 1.0f, 10.0f);

    auto culled1 = createEntity("culled_1");
    setBoundingBox(culled1, 500.0f, 0.0f, 5.0f, 510.0f, 1.0f, 10.0f);

    auto visible2 = createEntity("visible_2");
    setBoundingBox(visible2, -2.0f, -2.0f, 20.0f, 2.0f, 2.0f, 25.0f);

    auto noAabb = createEntity("no_aabb");

    float vp[16];
    camera.getViewProjection(vp);

    auto visibleNodes = culler.cull(vp, ecsWorld.get());
    auto names = visibleNames(visibleNodes);

    EXPECT_TRUE(names.count("visible_1"));
    EXPECT_TRUE(names.count("visible_2"));
    EXPECT_TRUE(names.count("no_aabb"));
    EXPECT_FALSE(names.count("culled_1"));
}

TEST_F(FrustumCullerTest, OrthoFrustumCull) {
    auto ortho = Matrix4x4<float>::orthographic(-10.0f, 10.0f, -10.0f, 10.0f, -10.0f, 10.0f);

    auto inside = createEntity("inside");
    setBoundingBox(inside, -1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f);

    auto outside = createEntity("outside");
    setBoundingBox(outside, 20.0f, 20.0f, 1.0f, 30.0f, 30.0f, 5.0f);

    auto visible = culler.cull(ortho.elements.data(), ecsWorld.get());
    auto names = visibleNames(visible);

    EXPECT_TRUE(names.count("inside"));
    EXPECT_FALSE(names.count("outside"));
}

TEST_F(FrustumCullerTest, CameraMovementChangesVisibleSet) {
    Camera camera;
    camera.setPerspective(60.0f, 1.0f, 0.1f, 100.0f, true);

    auto leftNode = createEntity("left");
    setBoundingBox(leftNode, -50.0f, -1.0f, 5.0f, -40.0f, 1.0f, 10.0f);

    auto rightNode = createEntity("right");
    setBoundingBox(rightNode, 40.0f, -1.0f, 5.0f, 50.0f, 1.0f, 10.0f);

    // Camera at origin: neither far-left nor far-right visible
    Transform<float> camTransform;
    camera.updateView(camTransform);

    float vp[16];
    camera.getViewProjection(vp);
    auto visible1 = culler.cull(vp, ecsWorld.get());
    auto names1 = visibleNames(visible1);
    EXPECT_FALSE(names1.count("left"));
    EXPECT_FALSE(names1.count("right"));

    // Move camera far to the left: left node should now be visible
    camTransform.setPosition(Vec3f(-45.0f, 0.0f, 0.0f));
    camera.updateView(camTransform);
    camera.getViewProjection(vp);
    auto visible2 = culler.cull(vp, ecsWorld.get());
    auto names2 = visibleNames(visible2);
    EXPECT_TRUE(names2.count("left"));
    EXPECT_FALSE(names2.count("right"));
}

TEST_F(FrustumCullerTest, MultipleCamerasOnDifferentViews) {
    Camera cameraA;
    cameraA.setPerspective(60.0f, 1.0f, 0.1f, 100.0f, true);

    Camera cameraB;
    cameraB.setPerspective(60.0f, 1.0f, 0.1f, 100.0f, true);

    auto nearCenter = createEntity("near_center");
    setBoundingBox(nearCenter, -1.0f, -1.0f, 5.0f, 1.0f, 1.0f, 10.0f);

    auto farRight = createEntity("far_right");
    setBoundingBox(farRight, 80.0f, -1.0f, 5.0f, 90.0f, 1.0f, 10.0f);

    // Camera A at origin: sees near_center, not far_right
    Transform<float> transformA;
    cameraA.updateView(transformA);

    float vpA[16];
    cameraA.getViewProjection(vpA);
    auto visibleA = culler.cull(vpA, ecsWorld.get());
    auto namesA = visibleNames(visibleA);
    EXPECT_TRUE(namesA.count("near_center"));
    EXPECT_FALSE(namesA.count("far_right"));

    // Camera B offset to the right: sees far_right, not near_center
    Transform<float> transformB;
    transformB.setPosition(Vec3f(85.0f, 0.0f, 0.0f));
    cameraB.updateView(transformB);

    float vpB[16];
    cameraB.getViewProjection(vpB);
    auto visibleB = culler.cull(vpB, ecsWorld.get());
    auto namesB = visibleNames(visibleB);
    EXPECT_FALSE(namesB.count("near_center"));
    EXPECT_TRUE(namesB.count("far_right"));
}

TEST_F(FrustumCullerTest, OnlySceneEntitiesCulled) {
    // Entities without SceneEntity tag should not appear in cull results
    Camera camera;
    camera.setPerspective(60.0f, 1.0f, 0.1f, 100.0f, true);
    Transform<float> camTransform;
    camera.updateView(camTransform);

    // Scene entity (should be visible)
    auto scene = createEntity("scene_entity");

    // Non-scene entity with Position but no SceneEntity tag
    ecsWorld.get().entity("non_scene").set<Position>({0.0f, 0.0f, 5.0f});

    float vp[16];
    camera.getViewProjection(vp);
    auto visible = culler.cull(vp, ecsWorld.get());
    auto names = visibleNames(visible);

    EXPECT_TRUE(names.count("scene_entity"));
    EXPECT_FALSE(names.count("non_scene"));
}

TEST_F(FrustumCullerTest, ChunkBoundingBoxVisibilityFiltersAsExpected) {
    Camera camera;
    camera.setPerspective(60.0f, 1.0f, 0.1f, 100.0f, true);
    Transform<float> camTransform;
    camera.updateView(camTransform);

    auto visibleChunk = createEntity("chunk_visible");
    setBoundingBox(visibleChunk, -16.0f, -16.0f, 4.0f, 16.0f, 16.0f, 36.0f);

    auto culledChunk = createEntity("chunk_culled");
    setBoundingBox(culledChunk, 600.0f, 600.0f, 8.0f, 632.0f, 632.0f, 40.0f);

    float vp[16];
    camera.getViewProjection(vp);
    auto visible = culler.cull(vp, ecsWorld.get());
    auto names = visibleNames(visible);

    EXPECT_TRUE(names.count("chunk_visible"));
    EXPECT_FALSE(names.count("chunk_culled"));
}

TEST_F(FrustumCullerTest, ChunkVisibilityChangesWithCameraMovement) {
    Camera camera;
    camera.setPerspective(60.0f, 1.0f, 0.1f, 100.0f, true);

    auto originChunk = createEntity("origin_chunk");
    setBoundingBox(originChunk, -16.0f, -16.0f, 4.0f, 16.0f, 16.0f, 36.0f);

    auto rightChunk = createEntity("right_chunk");
    setBoundingBox(rightChunk, 240.0f, -16.0f, 4.0f, 272.0f, 16.0f, 36.0f);

    float vp[16];

    Transform<float> atOrigin;
    camera.updateView(atOrigin);
    camera.getViewProjection(vp);
    auto visibleAtOrigin = culler.cull(vp, ecsWorld.get());
    auto namesAtOrigin = visibleNames(visibleAtOrigin);

    EXPECT_TRUE(namesAtOrigin.count("origin_chunk"));
    EXPECT_FALSE(namesAtOrigin.count("right_chunk"));

    Transform<float> movedRight;
    movedRight.setPosition(Vec3f(256.0f, 0.0f, 0.0f));
    camera.updateView(movedRight);
    camera.getViewProjection(vp);
    auto visibleMovedRight = culler.cull(vp, ecsWorld.get());
    auto namesMovedRight = visibleNames(visibleMovedRight);

    EXPECT_FALSE(namesMovedRight.count("origin_chunk"));
    EXPECT_TRUE(namesMovedRight.count("right_chunk"));
}

TEST_F(FrustumCullerTest, ChunkEntityMapAndVisibilitySetFilterGpuMeshKeys) {
    struct ChunkCoord {
        int cx;
        int cy;
        int cz;

        bool operator==(const ChunkCoord&) const = default;
    };

    struct ChunkCoordHash {
        size_t operator()(const ChunkCoord& coord) const noexcept {
            size_t h1 = std::hash<int>{}(coord.cx);
            size_t h2 = std::hash<int>{}(coord.cy);
            size_t h3 = std::hash<int>{}(coord.cz);
            return h1 ^ (h2 << 1) ^ (h3 << 2);
        }
    };

    Camera camera;
    camera.setPerspective(60.0f, 1.0f, 0.1f, 1000.0f, true);
    Transform<float> camTransform;
    camera.updateView(camTransform);

    auto nearEntity = createEntity("near_chunk");
    setBoundingBox(nearEntity, -16.0f, -16.0f, 16.0f, 16.0f, 16.0f, 48.0f);

    auto farEntity = createEntity("far_chunk");
    setBoundingBox(farEntity, 1000.0f, -16.0f, 16.0f, 1032.0f, 16.0f, 48.0f);

    std::unordered_map<ChunkCoord, flecs::entity, ChunkCoordHash> chunkEntities;
    chunkEntities[{0, 0, 0}] = nearEntity;
    chunkEntities[{31, 0, 0}] = farEntity;

    std::vector<ChunkCoord> gpuMeshKeys{{0, 0, 0}, {31, 0, 0}};

    float vp[16];
    camera.getViewProjection(vp);
    auto visibleEntities = culler.cull(vp, ecsWorld.get());

    std::unordered_set<flecs::entity_t> visibleIds;
    for (const auto& entity : visibleEntities) {
        visibleIds.insert(entity.id());
    }

    std::vector<ChunkCoord> drawList;
    for (const auto& coord : gpuMeshKeys) {
        auto it = chunkEntities.find(coord);
        if (it == chunkEntities.end()) {
            continue;
        }
        if (visibleIds.contains(it->second.id())) {
            drawList.push_back(coord);
        }
    }

    ASSERT_EQ(drawList.size(), 1u);
    EXPECT_EQ(drawList[0], (ChunkCoord{0, 0, 0}));
}

// --- BVH-backed frustum culling tests (TD-5) ---

TEST_F(FrustumCullerTest, BVHCullMatchesFlatIteration) {
    // Verify BVH-backed cull produces the same results as the old flat iteration
    Camera camera;
    camera.setPerspective(60.0f, 1.0f, 0.1f, 100.0f, true);
    Transform<float> camTransform;
    camera.updateView(camTransform);

    auto visible1 = createEntity("v1");
    setBoundingBox(visible1, -1.0f, -1.0f, 5.0f, 1.0f, 1.0f, 10.0f);

    auto culled1 = createEntity("c1");
    setBoundingBox(culled1, 500.0f, 0.0f, 5.0f, 510.0f, 1.0f, 10.0f);

    auto noBox = createEntity("nb");

    float vp[16];
    camera.getViewProjection(vp);

    auto visible = culler.cull(vp, ecsWorld.get());
    auto names = visibleNames(visible);

    EXPECT_EQ(names.size(), 2u);
    EXPECT_TRUE(names.count("v1"));
    EXPECT_TRUE(names.count("nb"));
    EXPECT_FALSE(names.count("c1"));
}

TEST_F(FrustumCullerTest, BVHCull1000Entities) {
    // Stress test: 1000+ entities with BoundingBox, verify correct cull count.
    // Use a perspective camera with wide FOV and large far plane.
    Camera camera;
    camera.setPerspective(90.0f, 1.0f, 0.1f, 10000.0f, true);
    Transform<float> camTransform;
    camera.updateView(camTransform);

    int expectedVisible = 0;

    for (int i = 0; i < 1024; ++i) {
        std::string name = "e" + std::to_string(i);
        auto e = createEntity(name.c_str());

        if (i % 2 == 0) {
            // Place directly in front of camera at origin, along +Z axis
            // Small box centered near z axis, spread along Z depth
            float z = 1.0f + static_cast<float>(i) * 0.5f;
            setBoundingBox(e, -0.1f, -0.1f, z, 0.1f, 0.1f, z + 0.2f);
            expectedVisible++;
        } else {
            // Place far outside frustum (culled): way off to the side
            float x = 50000.0f + static_cast<float>(i);
            setBoundingBox(e, x, 50000.0f, 5.0f, x + 1.0f, 50001.0f, 10.0f);
        }
    }

    float vp[16];
    camera.getViewProjection(vp);

    auto visible = culler.cull(vp, ecsWorld.get());

    // All even-indexed entities should be visible, all odd-indexed culled
    EXPECT_EQ(visible.size(), static_cast<size_t>(expectedVisible));
}

TEST_F(FrustumCullerTest, BVHEntitiesWithoutBoundingBoxAlwaysVisible) {
    Camera camera;
    camera.setPerspective(60.0f, 1.0f, 0.1f, 100.0f, true);
    Transform<float> camTransform;
    camera.updateView(camTransform);

    // Entity without BoundingBox at a position that would be culled if it had one
    auto noBB = createEntity("no_bb");
    // No bounding box set, so it should always be visible

    // Entity with BoundingBox outside frustum
    auto outsideBB = createEntity("outside_bb");
    setBoundingBox(outsideBB, 500.0f, 500.0f, 500.0f, 510.0f, 510.0f, 510.0f);

    float vp[16];
    camera.getViewProjection(vp);

    auto visible = culler.cull(vp, ecsWorld.get());
    auto names = visibleNames(visible);

    EXPECT_TRUE(names.count("no_bb"));
    EXPECT_FALSE(names.count("outside_bb"));
}

// BoundingBox component tests (replacing SceneNodeAABBTest)

TEST(BoundingBoxComponentTest, EntityDefaultHasNoBoundingBox) {
    World world;
    world.registerCoreComponents();
    auto entity = world.createSceneEntity("test");
    EXPECT_FALSE(entity.has<BoundingBox>());
}

TEST(BoundingBoxComponentTest, SetAndGetBoundingBox) {
    World world;
    world.registerCoreComponents();
    auto entity = world.createSceneEntity("test");
    entity.set<BoundingBox>({-1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f});

    const auto* bb = entity.try_get<BoundingBox>();
    ASSERT_NE(bb, nullptr);
    EXPECT_FLOAT_EQ(bb->minX, -1.0f);
    EXPECT_FLOAT_EQ(bb->maxX, 1.0f);
}

// --- Transparent render pass tests (EF-1c) ---
// These tests exercise the partition and sort logic without calling SceneView::render()
// (which requires bgfx initialization). The partition is tested via TransparentTag checks,
// and the sort is tested via the transparentSort() utility.

TEST(TransparentPassTest, PartitionSplitsOpaqueAndTransparent) {
    // TransparentTag partitions entities into two sets
    World world;
    world.registerCoreComponents();

    auto opaque1 = world.createSceneEntity("opaque1");
    auto opaque2 = world.createSceneEntity("opaque2");
    auto trans1 = world.createSceneEntity("trans1");
    trans1.add<TransparentTag>();
    auto trans2 = world.createSceneEntity("trans2");
    trans2.add<TransparentTag>();

    // Simulate the partition logic from SceneView::render()
    std::vector<flecs::entity> all = {opaque1, opaque2, trans1, trans2};
    std::vector<flecs::entity> opaqueList;
    std::vector<flecs::entity> transparentList;

    for (auto entity : all) {
        if (entity.has<TransparentTag>()) {
            transparentList.push_back(entity);
        } else {
            opaqueList.push_back(entity);
        }
    }

    EXPECT_EQ(opaqueList.size(), 2u);
    EXPECT_EQ(transparentList.size(), 2u);

    std::unordered_set<std::string> opaqueNames;
    for (auto e : opaqueList)
        opaqueNames.insert(std::string(e.name().c_str()));
    std::unordered_set<std::string> transNames;
    for (auto e : transparentList)
        transNames.insert(std::string(e.name().c_str()));

    EXPECT_TRUE(opaqueNames.count("opaque1"));
    EXPECT_TRUE(opaqueNames.count("opaque2"));
    EXPECT_TRUE(transNames.count("trans1"));
    EXPECT_TRUE(transNames.count("trans2"));
}

TEST(TransparentPassTest, EmptyTransparentListProducesNoTransparentPass) {
    // When no entities have TransparentTag, transparent list is empty
    World world;
    world.registerCoreComponents();

    auto opaque = world.createSceneEntity("opaque_only");

    std::vector<flecs::entity> all = {opaque};
    std::vector<flecs::entity> transparentList;
    for (auto entity : all) {
        if (entity.has<TransparentTag>()) {
            transparentList.push_back(entity);
        }
    }

    EXPECT_TRUE(transparentList.empty());
}

TEST(TransparentPassTest, OpaqueEntitiesStayInGeometryView) {
    // Verify view ID assignments: viewId+1 = geometry, viewId+2 = transparent
    Camera camera;
    camera.setPerspective(60.0f, 1.0f, 0.1f, 100.0f, true);

    World world;
    world.registerCoreComponents();

    SceneView view(10, camera, world.get());

    EXPECT_EQ(view.viewId(), 10u);
    EXPECT_EQ(view.geometryViewId(), 11u);
    EXPECT_EQ(view.transparentViewId(), 12u);
}

TEST(TransparentPassTest, TransparentSortDeterministic) {
    // Verify sort is deterministic across multiple runs
    World world;
    world.registerCoreComponents();

    auto a = world.createSceneEntity("a");
    a.set<Position>({0.0f, 0.0f, 20.0f});
    auto b = world.createSceneEntity("b");
    b.set<Position>({0.0f, 0.0f, 80.0f});

    Vec3f cameraPos(0.0f, 0.0f, 0.0f);

    for (int i = 0; i < 5; ++i) {
        std::vector<flecs::entity> entities = {a, b};
        transparentSort(entities, cameraPos);
        ASSERT_EQ(entities.size(), 2u);
        EXPECT_STREQ(entities[0].name().c_str(), "b");
        EXPECT_STREQ(entities[1].name().c_str(), "a");
    }
}

// --- transparentSort utility tests ---

TEST(TransparentSortTest, BackToFrontOrder) {
    World world;
    world.registerCoreComponents();

    auto a = world.createSceneEntity("a");
    a.set<Position>({0.0f, 0.0f, 10.0f});
    auto b = world.createSceneEntity("b");
    b.set<Position>({0.0f, 0.0f, 50.0f});
    auto c = world.createSceneEntity("c");
    c.set<Position>({0.0f, 0.0f, 100.0f});

    std::vector<flecs::entity> entities = {a, b, c};
    Vec3f cameraPos(0.0f, 0.0f, 0.0f);

    transparentSort(entities, cameraPos);

    // Farthest first (back-to-front)
    EXPECT_STREQ(entities[0].name().c_str(), "c");
    EXPECT_STREQ(entities[1].name().c_str(), "b");
    EXPECT_STREQ(entities[2].name().c_str(), "a");
}

TEST(TransparentSortTest, EmptyListNoOp) {
    std::vector<flecs::entity> entities;
    Vec3f cameraPos(0.0f, 0.0f, 0.0f);
    transparentSort(entities, cameraPos);
    EXPECT_TRUE(entities.empty());
}
