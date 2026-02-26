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

    auto visible = FrustumCuller::cull(vp, ecsWorld.get());

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

    auto visible = FrustumCuller::cull(vp, ecsWorld.get());
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

    auto visible = FrustumCuller::cull(vp, ecsWorld.get());
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

    auto visible = FrustumCuller::cull(vp, ecsWorld.get());
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

    auto visible = FrustumCuller::cull(vp, ecsWorld.get());
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

    auto visibleNodes = FrustumCuller::cull(vp, ecsWorld.get());
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

    auto visible = FrustumCuller::cull(ortho.elements.data(), ecsWorld.get());
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
    auto visible1 = FrustumCuller::cull(vp, ecsWorld.get());
    auto names1 = visibleNames(visible1);
    EXPECT_FALSE(names1.count("left"));
    EXPECT_FALSE(names1.count("right"));

    // Move camera far to the left: left node should now be visible
    camTransform.setPosition(Vec3f(-45.0f, 0.0f, 0.0f));
    camera.updateView(camTransform);
    camera.getViewProjection(vp);
    auto visible2 = FrustumCuller::cull(vp, ecsWorld.get());
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
    auto visibleA = FrustumCuller::cull(vpA, ecsWorld.get());
    auto namesA = visibleNames(visibleA);
    EXPECT_TRUE(namesA.count("near_center"));
    EXPECT_FALSE(namesA.count("far_right"));

    // Camera B offset to the right: sees far_right, not near_center
    Transform<float> transformB;
    transformB.setPosition(Vec3f(85.0f, 0.0f, 0.0f));
    cameraB.updateView(transformB);

    float vpB[16];
    cameraB.getViewProjection(vpB);
    auto visibleB = FrustumCuller::cull(vpB, ecsWorld.get());
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
    auto visible = FrustumCuller::cull(vp, ecsWorld.get());
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
    auto visible = FrustumCuller::cull(vp, ecsWorld.get());
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
    auto visibleAtOrigin = FrustumCuller::cull(vp, ecsWorld.get());
    auto namesAtOrigin = visibleNames(visibleAtOrigin);

    EXPECT_TRUE(namesAtOrigin.count("origin_chunk"));
    EXPECT_FALSE(namesAtOrigin.count("right_chunk"));

    Transform<float> movedRight;
    movedRight.setPosition(Vec3f(256.0f, 0.0f, 0.0f));
    camera.updateView(movedRight);
    camera.getViewProjection(vp);
    auto visibleMovedRight = FrustumCuller::cull(vp, ecsWorld.get());
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
    auto visibleEntities = FrustumCuller::cull(vp, ecsWorld.get());

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
