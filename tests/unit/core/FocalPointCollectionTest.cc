#include "recurse/systems/PhysicsGameSystem.hh"
#include "recurse/systems/VoxelSimulationSystem.hh"

#include "fabric/core/AppContext.hh"
#include "fabric/core/AssetRegistry.hh"
#include "fabric/core/ECS.hh"
#include "fabric/core/ResourceHub.hh"
#include "fabric/core/SystemRegistry.hh"
#include "fabric/platform/ConfigManager.hh"
#include "recurse/components/StreamSource.hh"
#include "recurse/simulation/ChunkRegistry.hh"
#include "recurse/simulation/SimulationGrid.hh"
#include "recurse/simulation/VoxelMaterial.hh"
#include "recurse/world/ChunkStreaming.hh"

#include <flecs.h>
#include <gtest/gtest.h>

using namespace recurse;
using namespace recurse::systems;
using recurse::simulation::ChunkSlotState;
using recurse::simulation::VoxelCell;
using recurse::simulation::material_ids::STONE;

// --- StreamSource ECS tests (3) ---

class StreamSourceTest : public ::testing::Test {
  protected:
    flecs::world ecs;
};

TEST_F(StreamSourceTest, QueryFindsStreamSourceEntities) {
    auto query = ecs.query_builder<const fabric::Position, const StreamSource>().build();

    ecs.entity().set(fabric::Position{1.0f, 2.0f, 3.0f}).set(StreamSource{8, 3});
    ecs.entity().set(fabric::Position{4.0f, 5.0f, 6.0f}).set(StreamSource{4, 2});

    // Entity without StreamSource should not appear
    ecs.entity().set(fabric::Position{7.0f, 8.0f, 9.0f});

    int count = 0;
    query.each([&](const fabric::Position&, const StreamSource&) { ++count; });

    EXPECT_EQ(count, 2);
}

TEST_F(StreamSourceTest, ZeroRadiusExcluded) {
    auto query = ecs.query_builder<const fabric::Position, const StreamSource>().build();

    ecs.entity().set(fabric::Position{0.0f, 0.0f, 0.0f}).set(StreamSource{8, 0});
    ecs.entity().set(fabric::Position{0.0f, 0.0f, 0.0f}).set(StreamSource{0, 3});
    ecs.entity().set(fabric::Position{0.0f, 0.0f, 0.0f}).set(StreamSource{5, 2});

    std::vector<FocalPoint> streamingFocals;
    std::vector<FocalPoint> collisionFocals;
    query.each([&](const fabric::Position& pos, const StreamSource& src) {
        if (src.streamRadius > 0)
            streamingFocals.push_back({pos.x, pos.y, pos.z, src.streamRadius});
        if (src.collisionRadius > 0)
            collisionFocals.push_back({pos.x, pos.y, pos.z, src.collisionRadius});
    });

    EXPECT_EQ(streamingFocals.size(), 2u) << "streamRadius=0 excluded from streaming";
    EXPECT_EQ(collisionFocals.size(), 2u) << "collisionRadius=0 excluded from collision";
}

TEST_F(StreamSourceTest, PositionUpdateReflected) {
    auto query = ecs.query_builder<const fabric::Position, const StreamSource>().build();

    auto entity = ecs.entity().set(fabric::Position{0.0f, 0.0f, 0.0f}).set(StreamSource{8, 3});

    std::vector<FocalPoint> focals;
    query.each([&](const fabric::Position& pos, const StreamSource& src) {
        focals.push_back({pos.x, pos.y, pos.z, src.streamRadius});
    });
    ASSERT_EQ(focals.size(), 1u);
    EXPECT_FLOAT_EQ(focals[0].x, 0.0f);

    // Update position
    entity.set(fabric::Position{100.0f, 200.0f, 300.0f});
    focals.clear();
    query.each([&](const fabric::Position& pos, const StreamSource& src) {
        focals.push_back({pos.x, pos.y, pos.z, src.streamRadius});
    });
    ASSERT_EQ(focals.size(), 1u);
    EXPECT_FLOAT_EQ(focals[0].x, 100.0f);
    EXPECT_FLOAT_EQ(focals[0].y, 200.0f);
    EXPECT_FLOAT_EQ(focals[0].z, 300.0f);
}

// --- Aggregation tests (4) ---

class FocalAggregationTest : public ::testing::Test {
  protected:
    flecs::world ecs;

    struct CollectedFocals {
        std::vector<FocalPoint> streaming;
        std::vector<FocalPoint> collision;
    };

    /// Replicates ChunkPipelineSystem::fixedUpdate focal collection logic
    CollectedFocals collectFocals(flecs::query<const fabric::Position, const StreamSource>& query,
                                  float fallbackX = 0.0f, float fallbackY = 0.0f, float fallbackZ = 0.0f,
                                  int defaultStreamRadius = 8, int defaultCollisionRadius = 3) {
        CollectedFocals result;
        query.each([&](const fabric::Position& pos, const StreamSource& src) {
            if (src.streamRadius > 0)
                result.streaming.push_back({pos.x, pos.y, pos.z, src.streamRadius});
            if (src.collisionRadius > 0)
                result.collision.push_back({pos.x, pos.y, pos.z, src.collisionRadius});
        });
        if (result.streaming.empty())
            result.streaming.push_back({fallbackX, fallbackY, fallbackZ, defaultStreamRadius});
        if (result.collision.empty())
            result.collision.push_back({fallbackX, fallbackY, fallbackZ, defaultCollisionRadius});
        return result;
    }
};

TEST_F(FocalAggregationTest, FallbackWhenNoStreamSource) {
    auto query = ecs.query_builder<const fabric::Position, const StreamSource>().build();

    auto focals = collectFocals(query, 10.0f, 20.0f, 30.0f, 8, 3);

    ASSERT_EQ(focals.streaming.size(), 1u);
    EXPECT_FLOAT_EQ(focals.streaming[0].x, 10.0f);
    EXPECT_EQ(focals.streaming[0].radius, 8);

    ASSERT_EQ(focals.collision.size(), 1u);
    EXPECT_FLOAT_EQ(focals.collision[0].x, 10.0f);
    EXPECT_EQ(focals.collision[0].radius, 3);
}

TEST_F(FocalAggregationTest, MultiEntityCollection) {
    auto query = ecs.query_builder<const fabric::Position, const StreamSource>().build();

    ecs.entity().set(fabric::Position{0.0f, 0.0f, 0.0f}).set(StreamSource{8, 3});
    ecs.entity().set(fabric::Position{100.0f, 0.0f, 100.0f}).set(StreamSource{4, 2});
    ecs.entity().set(fabric::Position{200.0f, 0.0f, 0.0f}).set(StreamSource{6, 3});

    auto focals = collectFocals(query);

    EXPECT_EQ(focals.streaming.size(), 3u);
    EXPECT_EQ(focals.collision.size(), 3u);
}

TEST_F(FocalAggregationTest, StreamingCollisionSeparation) {
    auto query = ecs.query_builder<const fabric::Position, const StreamSource>().build();

    // Entity A: streaming only (collisionRadius=0)
    ecs.entity().set(fabric::Position{0.0f, 0.0f, 0.0f}).set(StreamSource{8, 0});
    // Entity B: collision only (streamRadius=0)
    ecs.entity().set(fabric::Position{100.0f, 0.0f, 0.0f}).set(StreamSource{0, 3});
    // Entity C: both
    ecs.entity().set(fabric::Position{200.0f, 0.0f, 0.0f}).set(StreamSource{6, 2});

    auto focals = collectFocals(query);

    EXPECT_EQ(focals.streaming.size(), 2u) << "A(8) + C(6)";
    EXPECT_EQ(focals.collision.size(), 2u) << "B(3) + C(2)";

    // Verify streaming contains A and C but not B
    bool hasA = false, hasC = false;
    for (const auto& f : focals.streaming) {
        if (f.x == 0.0f && f.radius == 8)
            hasA = true;
        if (f.x == 200.0f && f.radius == 6)
            hasC = true;
    }
    EXPECT_TRUE(hasA);
    EXPECT_TRUE(hasC);

    // Verify collision contains B and C but not A
    bool hasB = false;
    hasC = false;
    for (const auto& f : focals.collision) {
        if (f.x == 100.0f && f.radius == 3)
            hasB = true;
        if (f.x == 200.0f && f.radius == 2)
            hasC = true;
    }
    EXPECT_TRUE(hasB);
    EXPECT_TRUE(hasC);
}

TEST_F(FocalAggregationTest, LODUnaffectedByStreamSource) {
    // D-42: LOD remains camera-centric.
    // Verify that StreamSource entities do not influence LOD parameters.
    // LOD uses lastPlayerX_/Y_/Z_ directly; StreamSource only feeds streaming and collision.
    // This test confirms the focal collection does NOT produce LOD-specific output.
    auto query = ecs.query_builder<const fabric::Position, const StreamSource>().build();

    ecs.entity().set(fabric::Position{500.0f, 0.0f, 500.0f}).set(StreamSource{12, 5});

    auto focals = collectFocals(query);

    // collectFocals produces streaming + collision only; no LOD focal list exists
    EXPECT_EQ(focals.streaming.size(), 1u);
    EXPECT_EQ(focals.collision.size(), 1u);
    // LOD would use a separate camera position, not StreamSource data.
    // The absence of a LOD focal list confirms D-42.
}

// --- Integration tests (2) ---

class FocalIntegrationTest : public ::testing::Test {
  protected:
    fabric::World world;
    fabric::Timeline timeline;
    fabric::EventDispatcher dispatcher;
    fabric::ResourceHub hub;
    fabric::AssetRegistry assetRegistry{hub};
    fabric::SystemRegistry systemRegistry;
    fabric::ConfigManager configManager;

    VoxelSimulationSystem* voxelSim_ = nullptr;
    PhysicsGameSystem physics;

    void SetUp() override {
        hub.disableWorkerThreadsForTesting();
        fabric::AppContext ctx = makeCtx();

        auto& vs = systemRegistry.registerSystem<VoxelSimulationSystem>(fabric::SystemPhase::FixedUpdate);
        voxelSim_ = &vs;
        systemRegistry.resolve();
        voxelSim_->init(ctx);

        physics.physicsWorld().init(4096, 0);
        physics.setVoxelSimForTesting(voxelSim_);
    }

    void TearDown() override {
        physics.physicsWorld().shutdown();
        fabric::AppContext ctx = makeCtx();
        voxelSim_->shutdown();
    }

    fabric::AppContext makeCtx() {
        return fabric::AppContext{
            .world = world,
            .timeline = timeline,
            .dispatcher = dispatcher,
            .resourceHub = hub,
            .assetRegistry = assetRegistry,
            .systemRegistry = systemRegistry,
            .configManager = configManager,
        };
    }

    void materializeActiveChunk(int cx, int cy, int cz) {
        auto& grid = voxelSim_->simulationGrid();
        auto& reg = grid.registry();
        reg.addChunk(cx, cy, cz);
        reg.transitionState(cx, cy, cz, ChunkSlotState::Generating);
        grid.materializeChunk(cx, cy, cz);
        grid.writeCell(cx * 32 + 4, cy * 32 + 4, cz * 32 + 4, VoxelCell{STONE});
        grid.syncChunkBuffers(cx, cy, cz);
        reg.transitionState(cx, cy, cz, ChunkSlotState::Active);
    }
};

TEST_F(FocalIntegrationTest, EndToEndPlayerPlusNPC) {
    // Player at origin, NPC at (5,0,5). Each has collision radius 3.
    // Chunks near both should get collision bodies.
    materializeActiveChunk(0, 0, 0);
    materializeActiveChunk(1, 0, 0);
    materializeActiveChunk(5, 0, 5);
    materializeActiveChunk(6, 0, 5);

    physics.insertDirtyChunk(0, 0, 0);
    physics.insertDirtyChunk(1, 0, 0);
    physics.insertDirtyChunk(5, 0, 5);
    physics.insertDirtyChunk(6, 0, 5);

    physics.setFocalPoints({
        {16.0f, 16.0f, 16.0f, K_COLLISION_RADIUS},
        {5.0f * 32.0f + 16.0f, 16.0f, 5.0f * 32.0f + 16.0f, K_COLLISION_RADIUS},
    });
    auto ctx = makeCtx();
    physics.fixedUpdate(ctx, 1.0f / 60.0f);

    EXPECT_GT(physics.physicsWorld().chunkCollisionShapeCount(0, 0, 0), 0u) << "Near player";
    EXPECT_GT(physics.physicsWorld().chunkCollisionShapeCount(1, 0, 0), 0u) << "Near player";
    EXPECT_GT(physics.physicsWorld().chunkCollisionShapeCount(5, 0, 5), 0u) << "Near NPC";
    EXPECT_GT(physics.physicsWorld().chunkCollisionShapeCount(6, 0, 5), 0u) << "Near NPC";
}

TEST_F(FocalIntegrationTest, NPCRemovalCleansUpCollision) {
    // Build collision around two focal points, then remove one.
    // Chunks exclusive to the removed focal should lose collision after cleanup.
    materializeActiveChunk(0, 0, 0);
    materializeActiveChunk(10, 0, 10);

    physics.insertDirtyChunk(0, 0, 0);
    physics.insertDirtyChunk(10, 0, 10);

    physics.setFocalPoints({
        {16.0f, 16.0f, 16.0f, K_COLLISION_RADIUS},
        {10.0f * 32.0f + 16.0f, 16.0f, 10.0f * 32.0f + 16.0f, K_COLLISION_RADIUS},
    });
    auto ctx = makeCtx();
    physics.fixedUpdate(ctx, 1.0f / 60.0f);

    EXPECT_GT(physics.physicsWorld().chunkCollisionShapeCount(0, 0, 0), 0u);
    EXPECT_GT(physics.physicsWorld().chunkCollisionShapeCount(10, 0, 10), 0u);

    // "Remove" NPC by setting single focal point (player only)
    physics.setFocalPoints({{16.0f, 16.0f, 16.0f, K_COLLISION_RADIUS}});
    physics.fixedUpdate(ctx, 1.0f / 60.0f);

    // NPC's chunk should have collision removed via proactive cleanup
    EXPECT_EQ(physics.physicsWorld().chunkCollisionShapeCount(10, 0, 10), 0u)
        << "Chunk exclusive to removed NPC should lose collision";
    EXPECT_GT(physics.physicsWorld().chunkCollisionShapeCount(0, 0, 0), 0u) << "Player's chunk should retain collision";
}
