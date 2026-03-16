#include "recurse/simulation/ChunkState.hh"
#include "recurse/systems/PhysicsGameSystem.hh"
#include "recurse/systems/VoxelSimulationSystem.hh"

#include "recurse/config/RecurseConfig.hh"

#include "fabric/core/AppContext.hh"
#include "fabric/core/SystemRegistry.hh"
#include "fabric/platform/ConfigManager.hh"
#include "fabric/resource/AssetRegistry.hh"
#include "fabric/resource/ResourceHub.hh"
#include "recurse/simulation/ChunkRegistry.hh"
#include "recurse/simulation/SimulationGrid.hh"
#include "recurse/simulation/VoxelMaterial.hh"

#include <gtest/gtest.h>

using namespace recurse::systems;
using recurse::simulation::ChunkSlotState;
using recurse::simulation::SimulationGrid;
using recurse::simulation::VoxelCell;
using recurse::simulation::material_ids::STONE;

class CollisionRadiusTest : public ::testing::Test {
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
        using namespace recurse::simulation;
        auto& grid = voxelSim_->simulationGrid();
        auto& reg = grid.registry();
        auto absent = addChunkRef(reg, cx, cy, cz);
        auto generating = transition<Absent, Generating>(absent, reg);
        grid.materializeChunk(cx, cy, cz);
        grid.writeCell(cx * 32 + 4, cy * 32 + 4, cz * 32 + 4, VoxelCell{STONE});
        grid.syncChunkBuffers(cx, cy, cz);
        transition<Generating, Active>(generating, reg);
    }
};

// --- Collision radius (5 tests) ---

TEST_F(CollisionRadiusTest, BeyondRadiusFilteredFromDirtySet) {
    materializeActiveChunk(0, 0, 0);
    materializeActiveChunk(recurse::RecurseConfig::K_DEFAULT_COLLISION_RADIUS + 1, 0, 0);

    physics.insertDirtyChunk(0, 0, 0);
    physics.insertDirtyChunk(recurse::RecurseConfig::K_DEFAULT_COLLISION_RADIUS + 1, 0, 0);

    physics.setFocalPoints({{0.0f, 0.0f, 0.0f, recurse::RecurseConfig::K_DEFAULT_COLLISION_RADIUS}});
    auto ctx = makeCtx();
    physics.fixedUpdate(ctx, 1.0f / 60.0f);

    EXPECT_GT(physics.physicsWorld().chunkCollisionShapeCount(0, 0, 0), 0u);
    EXPECT_EQ(
        physics.physicsWorld().chunkCollisionShapeCount(recurse::RecurseConfig::K_DEFAULT_COLLISION_RADIUS + 1, 0, 0),
        0u);
}

TEST_F(CollisionRadiusTest, OverflowDropBeyondRadius) {
    for (int x = -2; x <= 2; ++x)
        for (int z = -2; z <= 2; ++z)
            materializeActiveChunk(x, 0, z);

    materializeActiveChunk(recurse::RecurseConfig::K_DEFAULT_COLLISION_RADIUS + 2, 0, 0);

    for (int x = -2; x <= 2; ++x)
        for (int z = -2; z <= 2; ++z)
            physics.insertDirtyChunk(x, 0, z);
    physics.insertDirtyChunk(recurse::RecurseConfig::K_DEFAULT_COLLISION_RADIUS + 2, 0, 0);

    physics.setFocalPoints({{0.0f, 0.0f, 0.0f, recurse::RecurseConfig::K_DEFAULT_COLLISION_RADIUS}});
    auto ctx = makeCtx();

    // Process all frames until dirty set drains
    for (int i = 0; i < 10; ++i)
        physics.fixedUpdate(ctx, 1.0f / 60.0f);

    EXPECT_EQ(
        physics.physicsWorld().chunkCollisionShapeCount(recurse::RecurseConfig::K_DEFAULT_COLLISION_RADIUS + 2, 0, 0),
        0u);
    EXPECT_TRUE(physics.dirtyChunks().empty());
}

TEST_F(CollisionRadiusTest, ProactiveCleanupOnFocalMove) {
    materializeActiveChunk(0, 0, 0);
    materializeActiveChunk(10, 0, 0);

    physics.insertDirtyChunk(0, 0, 0);
    physics.insertDirtyChunk(10, 0, 0);

    // First: focal at origin; chunk (10,0,0) beyond radius, only (0,0,0) gets collision
    physics.setFocalPoints({{0.0f, 0.0f, 0.0f, recurse::RecurseConfig::K_DEFAULT_COLLISION_RADIUS}});
    auto ctx = makeCtx();
    physics.fixedUpdate(ctx, 1.0f / 60.0f);
    EXPECT_GT(physics.physicsWorld().chunkCollisionShapeCount(0, 0, 0), 0u);

    // Move focal far away from origin
    physics.setFocalPoints({{10.0f * 32.0f + 16.0f, 16.0f, 16.0f, recurse::RecurseConfig::K_DEFAULT_COLLISION_RADIUS}});
    physics.fixedUpdate(ctx, 1.0f / 60.0f);

    // Origin chunk should have had its collision bodies removed (proactive cleanup)
    EXPECT_EQ(physics.physicsWorld().chunkCollisionShapeCount(0, 0, 0), 0u);
}

TEST_F(CollisionRadiusTest, ReDirtyOnFocalReturn) {
    materializeActiveChunk(0, 0, 0);

    physics.insertDirtyChunk(0, 0, 0);
    physics.setFocalPoints({{0.0f, 0.0f, 0.0f, recurse::RecurseConfig::K_DEFAULT_COLLISION_RADIUS}});
    auto ctx = makeCtx();
    physics.fixedUpdate(ctx, 1.0f / 60.0f);
    EXPECT_GT(physics.physicsWorld().chunkCollisionShapeCount(0, 0, 0), 0u);

    // Move far away; proactive cleanup removes collision
    physics.setFocalPoints({{100.0f * 32.0f, 0.0f, 0.0f, recurse::RecurseConfig::K_DEFAULT_COLLISION_RADIUS}});
    physics.fixedUpdate(ctx, 1.0f / 60.0f);
    EXPECT_EQ(physics.physicsWorld().chunkCollisionShapeCount(0, 0, 0), 0u);

    // Return to origin; re-dirty scan adds to dirty set, processed next frame
    physics.setFocalPoints({{0.0f, 0.0f, 0.0f, recurse::RecurseConfig::K_DEFAULT_COLLISION_RADIUS}});
    physics.fixedUpdate(ctx, 1.0f / 60.0f);
    physics.fixedUpdate(ctx, 1.0f / 60.0f);
    EXPECT_GT(physics.physicsWorld().chunkCollisionShapeCount(0, 0, 0), 0u);
}

TEST_F(CollisionRadiusTest, SteadyStateNoRedundantWork) {
    materializeActiveChunk(0, 0, 0);

    physics.insertDirtyChunk(0, 0, 0);
    physics.setFocalPoints({{0.0f, 0.0f, 0.0f, recurse::RecurseConfig::K_DEFAULT_COLLISION_RADIUS}});
    auto ctx = makeCtx();
    physics.fixedUpdate(ctx, 1.0f / 60.0f);
    EXPECT_GT(physics.physicsWorld().chunkCollisionShapeCount(0, 0, 0), 0u);

    // Second frame: no dirty chunks should remain, no work done
    physics.fixedUpdate(ctx, 1.0f / 60.0f);
    EXPECT_TRUE(physics.dirtyChunks().empty());
}

// --- Physics multi-focal (5 tests) ---

TEST_F(CollisionRadiusTest, SingleFocalCompat) {
    materializeActiveChunk(0, 0, 0);
    materializeActiveChunk(1, 0, 0);

    physics.insertDirtyChunk(0, 0, 0);
    physics.insertDirtyChunk(1, 0, 0);

    physics.setFocalPoints({{16.0f, 16.0f, 16.0f, recurse::RecurseConfig::K_DEFAULT_COLLISION_RADIUS}});
    auto ctx = makeCtx();
    physics.fixedUpdate(ctx, 1.0f / 60.0f);

    EXPECT_GT(physics.physicsWorld().chunkCollisionShapeCount(0, 0, 0), 0u);
    EXPECT_GT(physics.physicsWorld().chunkCollisionShapeCount(1, 0, 0), 0u);
}

TEST_F(CollisionRadiusTest, TwoFocalSortByNearest) {
    // Two focal points at different positions. Budget=8. 20 chunks dirty.
    // Nearest chunks to either focal should be processed first.
    int coords[][3] = {
        {0, 0, 0}, {1, 0, 0},   {-1, 0, 0},  {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1},
        {5, 0, 5}, {6, 0, 5},   {4, 0, 5},   {5, 1, 5}, {5, -1, 5}, {5, 0, 6}, {5, 0, 4},
        {2, 0, 2}, {-1, -1, 0}, {-1, 0, -1}, {6, 0, 6}, {4, 0, 4},  {3, 0, 3},
    };
    constexpr int N = 20;

    for (int i = 0; i < N; ++i)
        materializeActiveChunk(coords[i][0], coords[i][1], coords[i][2]);
    for (int i = 0; i < N; ++i)
        physics.insertDirtyChunk(coords[i][0], coords[i][1], coords[i][2]);

    physics.setFocalPoints({
        {0.0f, 0.0f, 0.0f, recurse::RecurseConfig::K_DEFAULT_COLLISION_RADIUS},
        {5.0f * 32.0f + 16.0f, 16.0f, 5.0f * 32.0f + 16.0f, recurse::RecurseConfig::K_DEFAULT_COLLISION_RADIUS},
    });
    auto ctx = makeCtx();
    physics.fixedUpdate(ctx, 1.0f / 60.0f);

    // Chunks immediately adjacent to either focal should be rebuilt
    EXPECT_GT(physics.physicsWorld().chunkCollisionShapeCount(0, 0, 0), 0u) << "Origin focal's center chunk";
    EXPECT_GT(physics.physicsWorld().chunkCollisionShapeCount(5, 0, 5), 0u) << "Second focal's center chunk";
}

TEST_F(CollisionRadiusTest, RadiusFilterMultiFocal) {
    // Chunk at (20,0,0) is beyond both focal points' radii
    materializeActiveChunk(0, 0, 0);
    materializeActiveChunk(5, 0, 5);
    materializeActiveChunk(20, 0, 0);

    physics.insertDirtyChunk(0, 0, 0);
    physics.insertDirtyChunk(5, 0, 5);
    physics.insertDirtyChunk(20, 0, 0);

    physics.setFocalPoints({
        {0.0f, 0.0f, 0.0f, recurse::RecurseConfig::K_DEFAULT_COLLISION_RADIUS},
        {5.0f * 32.0f + 16.0f, 16.0f, 5.0f * 32.0f + 16.0f, recurse::RecurseConfig::K_DEFAULT_COLLISION_RADIUS},
    });
    auto ctx = makeCtx();
    physics.fixedUpdate(ctx, 1.0f / 60.0f);

    EXPECT_GT(physics.physicsWorld().chunkCollisionShapeCount(0, 0, 0), 0u);
    EXPECT_GT(physics.physicsWorld().chunkCollisionShapeCount(5, 0, 5), 0u);
    EXPECT_EQ(physics.physicsWorld().chunkCollisionShapeCount(20, 0, 0), 0u)
        << "Chunk beyond all focal radii should have no collision";
}

TEST_F(CollisionRadiusTest, WithinAnySourceRetained) {
    // Chunk (2,0,0) is within focal A's radius but outside focal B's
    materializeActiveChunk(2, 0, 0);

    physics.insertDirtyChunk(2, 0, 0);

    physics.setFocalPoints({
        {0.0f, 0.0f, 0.0f, recurse::RecurseConfig::K_DEFAULT_COLLISION_RADIUS},
        {10.0f * 32.0f, 0.0f, 0.0f, recurse::RecurseConfig::K_DEFAULT_COLLISION_RADIUS},
    });
    auto ctx = makeCtx();
    physics.fixedUpdate(ctx, 1.0f / 60.0f);

    EXPECT_GT(physics.physicsWorld().chunkCollisionShapeCount(2, 0, 0), 0u)
        << "Chunk within any focal radius should be retained";
}

TEST_F(CollisionRadiusTest, OverflowDropMultiFocal) {
    // Fill budget from two focal points; overflow should persist, far chunks dropped
    int nearA[][3] = {{0, 0, 0}, {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
    int nearB[][3] = {{8, 0, 0}, {9, 0, 0}, {7, 0, 0}, {8, 1, 0}, {8, 0, 1}};
    int farChunk[] = {20, 0, 0};

    for (auto& c : nearA)
        materializeActiveChunk(c[0], c[1], c[2]);
    for (auto& c : nearB)
        materializeActiveChunk(c[0], c[1], c[2]);
    materializeActiveChunk(farChunk[0], farChunk[1], farChunk[2]);

    for (auto& c : nearA)
        physics.insertDirtyChunk(c[0], c[1], c[2]);
    for (auto& c : nearB)
        physics.insertDirtyChunk(c[0], c[1], c[2]);
    physics.insertDirtyChunk(farChunk[0], farChunk[1], farChunk[2]);

    physics.setFocalPoints({
        {0.0f, 0.0f, 0.0f, recurse::RecurseConfig::K_DEFAULT_COLLISION_RADIUS},
        {8.0f * 32.0f + 16.0f, 16.0f, 16.0f, recurse::RecurseConfig::K_DEFAULT_COLLISION_RADIUS},
    });
    auto ctx = makeCtx();
    physics.fixedUpdate(ctx, 1.0f / 60.0f);

    // Budget is 8; 10 near chunks + 1 far. Far chunk beyond both radii: dropped entirely.
    EXPECT_EQ(physics.physicsWorld().chunkCollisionShapeCount(farChunk[0], farChunk[1], farChunk[2]), 0u);

    // After enough frames, all near chunks should be rebuilt
    for (int i = 0; i < 5; ++i)
        physics.fixedUpdate(ctx, 1.0f / 60.0f);

    int rebuilt = 0;
    for (auto& c : nearA)
        if (physics.physicsWorld().chunkCollisionShapeCount(c[0], c[1], c[2]) > 0)
            ++rebuilt;
    for (auto& c : nearB)
        if (physics.physicsWorld().chunkCollisionShapeCount(c[0], c[1], c[2]) > 0)
            ++rebuilt;
    EXPECT_EQ(rebuilt, 10);
    EXPECT_TRUE(physics.dirtyChunks().empty());
}
