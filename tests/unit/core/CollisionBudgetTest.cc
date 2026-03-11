#include "recurse/systems/PhysicsGameSystem.hh"
#include "recurse/systems/VoxelSimulationSystem.hh"

#include "fabric/core/AppContext.hh"
#include "fabric/core/AssetRegistry.hh"
#include "fabric/core/ResourceHub.hh"
#include "fabric/core/SystemRegistry.hh"
#include "fabric/platform/ConfigManager.hh"
#include "recurse/simulation/ChunkRegistry.hh"
#include "recurse/simulation/SimulationGrid.hh"
#include "recurse/simulation/VoxelMaterial.hh"

#include <gtest/gtest.h>

using namespace recurse::systems;
using recurse::simulation::ChunkSlotState;
using recurse::simulation::SimulationGrid;
using recurse::simulation::VoxelCell;
using recurse::simulation::material_ids::STONE;

class CollisionBudgetTest : public ::testing::Test {
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
        // Write at least one solid voxel so collision shapes are generated
        grid.writeCell(cx * 32 + 4, cy * 32 + 4, cz * 32 + 4, VoxelCell{STONE});
        grid.syncChunkBuffers(cx, cy, cz);
        reg.transitionState(cx, cy, cz, ChunkSlotState::Active);
    }

    void addChunkInState(int cx, int cy, int cz, ChunkSlotState state) {
        auto& grid = voxelSim_->simulationGrid();
        auto& reg = grid.registry();
        reg.addChunk(cx, cy, cz);
        reg.transitionState(cx, cy, cz, ChunkSlotState::Generating);
        grid.materializeChunk(cx, cy, cz);
        if (state == ChunkSlotState::Active) {
            grid.syncChunkBuffers(cx, cy, cz);
            reg.transitionState(cx, cy, cz, ChunkSlotState::Active);
        }
    }
};

TEST_F(CollisionBudgetTest, BudgetCapsProcessedChunksPerFrame) {
    // 20 chunks within collision radius (all dist_sq <= 4 from origin)
    int coords[][3] = {
        {0, 0, 0},   {1, 0, 0},  {-1, 0, 0}, {0, 1, 0},   {0, -1, 0},  {0, 0, 1},  {0, 0, -1},
        {1, 1, 0},   {1, -1, 0}, {-1, 1, 0}, {-1, -1, 0}, {1, 0, 1},   {1, 0, -1}, {-1, 0, 1},
        {-1, 0, -1}, {0, 1, 1},  {0, 1, -1}, {0, -1, 1},  {0, -1, -1}, {2, 0, 0},
    };
    constexpr int N = 20;

    for (int i = 0; i < N; ++i)
        materializeActiveChunk(coords[i][0], coords[i][1], coords[i][2]);

    for (int i = 0; i < N; ++i)
        physics.insertDirtyChunk(coords[i][0], coords[i][1], coords[i][2]);

    physics.setFocalPoints({{0.0f, 0.0f, 0.0f, K_COLLISION_RADIUS}});
    auto ctx = makeCtx();
    physics.fixedUpdate(ctx, 1.0f / 60.0f);

    int rebuilt = 0;
    for (int i = 0; i < N; ++i) {
        if (physics.physicsWorld().chunkCollisionShapeCount(coords[i][0], coords[i][1], coords[i][2]) > 0)
            ++rebuilt;
    }

    EXPECT_EQ(rebuilt, K_COLLISION_BUDGET_PER_FRAME);
    EXPECT_EQ(static_cast<int>(physics.dirtyChunks().size()), N - K_COLLISION_BUDGET_PER_FRAME);
}

TEST_F(CollisionBudgetTest, NearestChunksProcessedFirst) {
    physics.setFocalPoints({{5.0f * 32.0f + 16.0f, 16.0f, 5.0f * 32.0f + 16.0f, K_COLLISION_RADIUS}});

    int nearCoords[][3] = {{5, 0, 5}, {4, 0, 5}, {6, 0, 5}};
    int farCoords[][3] = {{0, 0, 0}, {10, 0, 10}, {9, 0, 9}, {1, 0, 1}};

    for (auto& c : nearCoords) {
        materializeActiveChunk(c[0], c[1], c[2]);
        physics.insertDirtyChunk(c[0], c[1], c[2]);
    }
    for (auto& c : farCoords) {
        materializeActiveChunk(c[0], c[1], c[2]);
        physics.insertDirtyChunk(c[0], c[1], c[2]);
    }

    auto ctx = makeCtx();
    physics.fixedUpdate(ctx, 1.0f / 60.0f);

    for (auto& c : nearCoords) {
        EXPECT_GT(physics.physicsWorld().chunkCollisionShapeCount(c[0], c[1], c[2]), 0u)
            << "Near chunk (" << c[0] << "," << c[1] << "," << c[2] << ") should be rebuilt";
    }
}

TEST_F(CollisionBudgetTest, NonActiveChunksFiltered) {
    materializeActiveChunk(0, 0, 0);
    addChunkInState(1, 0, 0, ChunkSlotState::Generating);

    physics.insertDirtyChunk(0, 0, 0);
    physics.insertDirtyChunk(1, 0, 0);

    physics.setFocalPoints({{0.0f, 0.0f, 0.0f, K_COLLISION_RADIUS}});
    auto ctx = makeCtx();
    physics.fixedUpdate(ctx, 1.0f / 60.0f);

    EXPECT_GT(physics.physicsWorld().chunkCollisionShapeCount(0, 0, 0), 0u);
    EXPECT_EQ(physics.physicsWorld().chunkCollisionShapeCount(1, 0, 0), 0u);
    EXPECT_TRUE(physics.dirtyChunks().empty());
}

TEST_F(CollisionBudgetTest, OverflowPersistsAcrossFrames) {
    // 12 chunks within collision radius (all dist_sq <= 4 from origin)
    int coords[][3] = {
        {0, 0, 0},  {1, 0, 0}, {-1, 0, 0}, {0, 1, 0},  {0, -1, 0},  {0, 0, 1},
        {0, 0, -1}, {1, 1, 0}, {1, -1, 0}, {-1, 1, 0}, {-1, -1, 0}, {2, 0, 0},
    };
    constexpr int N = 12;

    for (int i = 0; i < N; ++i)
        materializeActiveChunk(coords[i][0], coords[i][1], coords[i][2]);

    for (int i = 0; i < N; ++i)
        physics.insertDirtyChunk(coords[i][0], coords[i][1], coords[i][2]);

    physics.setFocalPoints({{0.0f, 0.0f, 0.0f, K_COLLISION_RADIUS}});
    auto ctx = makeCtx();

    physics.fixedUpdate(ctx, 1.0f / 60.0f);
    EXPECT_EQ(static_cast<int>(physics.dirtyChunks().size()), 4);

    int rebuilt = 0;
    for (int i = 0; i < N; ++i) {
        if (physics.physicsWorld().chunkCollisionShapeCount(coords[i][0], coords[i][1], coords[i][2]) > 0)
            ++rebuilt;
    }
    EXPECT_EQ(rebuilt, 8);

    physics.fixedUpdate(ctx, 1.0f / 60.0f);
    EXPECT_TRUE(physics.dirtyChunks().empty());

    rebuilt = 0;
    for (int i = 0; i < N; ++i) {
        if (physics.physicsWorld().chunkCollisionShapeCount(coords[i][0], coords[i][1], coords[i][2]) > 0)
            ++rebuilt;
    }
    EXPECT_EQ(rebuilt, N);
}
