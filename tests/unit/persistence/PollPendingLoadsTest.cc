#include "recurse/persistence/WorldSession.hh"

#include "fabric/core/AppContext.hh"
#include "fabric/core/SystemRegistry.hh"
#include "fabric/platform/ConfigManager.hh"
#include "fabric/platform/JobScheduler.hh"
#include "fabric/resource/AssetRegistry.hh"
#include "fabric/resource/ResourceHub.hh"
#include "recurse/persistence/FchkCodec.hh"
#include "recurse/persistence/SqliteChunkStore.hh"
#include "recurse/simulation/ChunkState.hh"
#include "recurse/simulation/SimulationGrid.hh"
#include "recurse/simulation/VoxelMaterial.hh"
#include "recurse/systems/VoxelSimulationSystem.hh"
#include "recurse/world/ChunkOps.hh"

#include <chrono>
#include <filesystem>
#include <gtest/gtest.h>

#include <flecs.h>

namespace fs = std::filesystem;

using namespace recurse::systems;
using recurse::simulation::VoxelCell;
using recurse::simulation::material_ids::STONE;

class PollPendingLoadsTest : public ::testing::Test {
  protected:
    fabric::World world;
    fabric::Timeline timeline;
    fabric::EventDispatcher dispatcher;
    fabric::ResourceHub hub;
    fabric::AssetRegistry assetRegistry{hub};
    fabric::SystemRegistry systemRegistry;
    fabric::ConfigManager configManager;
    fabric::JobScheduler scheduler;

    VoxelSimulationSystem* voxelSim_ = nullptr;
    std::unique_ptr<recurse::WorldSession> session_;
    fs::path tmpDir_;

    void SetUp() override {
        hub.disableWorkerThreadsForTesting();
        scheduler.disableForTesting();

        fabric::AppContext ctx = makeCtx();
        auto& vs = systemRegistry.registerSystem<VoxelSimulationSystem>(fabric::SystemPhase::FixedUpdate);
        voxelSim_ = &vs;
        systemRegistry.resolve();
        voxelSim_->init(ctx);

        tmpDir_ = fs::temp_directory_path() /
                  ("fabric_poll_test_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        fs::create_directories(tmpDir_);

        auto result = recurse::WorldSession::open(tmpDir_.string(), dispatcher, scheduler, world.get(), voxelSim_,
                                                  nullptr, nullptr, nullptr, nullptr);
        ASSERT_TRUE(result.isSuccess());
        session_ = std::move(result).value();
    }

    void TearDown() override {
        session_.reset();
        voxelSim_->shutdown();
        fs::remove_all(tmpDir_);
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

    void saveChunkToStore(int cx, int cy, int cz) {
        // Materialize a chunk in the simulation grid, write a known cell, encode and save
        materializeActiveChunk(cx, cy, cz);

        auto blob = session_->encodeChunkBlob(cx, cy, cz);
        session_->chunkStore()->saveChunk(cx, cy, cz, blob);

        // Remove the chunk from the grid so it can be async-loaded fresh
        voxelSim_->removeChunk(cx, cy, cz);
    }
};

TEST_F(PollPendingLoadsTest, EmptyPendingLoadsReturnsEmpty) {
    auto completions = session_->pollPendingLoads();
    EXPECT_TRUE(completions.empty());
}

TEST_F(PollPendingLoadsTest, SuccessfulLoadReturnsCompletedLoad) {
    saveChunkToStore(5, 0, 5);

    bool dispatched = session_->dispatchAsyncLoad(5, 0, 5);
    ASSERT_TRUE(dispatched);
    ASSERT_EQ(session_->pendingLoads().size(), 1u);

    // With scheduler disabled for testing, futures complete synchronously
    auto completions = session_->pollPendingLoads();
    ASSERT_EQ(completions.size(), 1u);
    EXPECT_EQ(completions[0].cx, 5);
    EXPECT_EQ(completions[0].cy, 0);
    EXPECT_EQ(completions[0].cz, 5);
    EXPECT_TRUE(completions[0].success);

    EXPECT_TRUE(session_->pendingLoads().empty());
}

TEST_F(PollPendingLoadsTest, LoadNonexistentChunkNotDispatched) {
    // dispatchAsyncLoad checks both persist-pending memory and storage before dispatching
    bool dispatched = session_->dispatchAsyncLoad(99, 99, 99);
    EXPECT_FALSE(dispatched);
    EXPECT_TRUE(session_->pendingLoads().empty());
}

TEST_F(PollPendingLoadsTest, PersistPendingChunkReloadsBeforeDbCommit) {
    constexpr int K_CX = 9;
    constexpr int K_CY = 0;
    constexpr int K_CZ = 9;

    materializeActiveChunk(K_CX, K_CY, K_CZ);
    ASSERT_FALSE(session_->chunkStore()->hasChunk(K_CX, K_CY, K_CZ));

    session_->submit(recurse::ops::PersistChunk{K_CX, K_CY, K_CZ});
    auto* saveService = session_->saveService();
    ASSERT_NE(saveService, nullptr);
    EXPECT_TRUE(saveService->hasPersistPending(K_CX, K_CY, K_CZ));

    voxelSim_->removeChunk(K_CX, K_CY, K_CZ);

    bool dispatched = session_->dispatchAsyncLoad(K_CX, K_CY, K_CZ);
    ASSERT_TRUE(dispatched);
    ASSERT_EQ(session_->pendingLoads().size(), 1u);

    auto completions = session_->pollPendingLoads();
    ASSERT_EQ(completions.size(), 1u);
    EXPECT_TRUE(completions[0].success);
    EXPECT_EQ(voxelSim_->simulationGrid().readCell(K_CX * 32 + 4, K_CY * 32 + 4, K_CZ * 32 + 4).materialId, STONE);

    EXPECT_TRUE(saveService->hasPersistPending(K_CX, K_CY, K_CZ));

    saveService->flush();

    EXPECT_FALSE(saveService->hasPersistPending(K_CX, K_CY, K_CZ));
    EXPECT_TRUE(session_->chunkStore()->hasChunk(K_CX, K_CY, K_CZ));
}

TEST_F(PollPendingLoadsTest, MaxCompletionsBudget) {
    for (int i = 0; i < 4; ++i)
        saveChunkToStore(i, 0, 0);

    for (int i = 0; i < 4; ++i) {
        bool dispatched = session_->dispatchAsyncLoad(i, 0, 0);
        ASSERT_TRUE(dispatched) << "chunk " << i;
    }
    ASSERT_EQ(session_->pendingLoads().size(), 4u);

    session_->setMaxLoadCompletions(2);
    auto completions = session_->pollPendingLoads();
    EXPECT_EQ(completions.size(), 2u);
    EXPECT_EQ(session_->pendingLoads().size(), 2u);

    // Poll remaining
    auto rest = session_->pollPendingLoads();
    EXPECT_EQ(rest.size(), 2u);
    EXPECT_TRUE(session_->pendingLoads().empty());
}

TEST_F(PollPendingLoadsTest, CancelledLoadExcluded) {
    saveChunkToStore(7, 0, 7);

    bool dispatched = session_->dispatchAsyncLoad(7, 0, 7);
    ASSERT_TRUE(dispatched);

    bool cancelled = session_->cancelPendingLoad(7, 0, 7);
    EXPECT_TRUE(cancelled);

    auto completions = session_->pollPendingLoads();
    // Cancelled loads are silently removed, not returned in completions
    EXPECT_TRUE(completions.empty());
    EXPECT_TRUE(session_->pendingLoads().empty());
}

TEST_F(PollPendingLoadsTest, LoadWithoutPaletteRebuildsAndReplacesStalePalette) {
    saveChunkToStore(8, 0, 8);

    ASSERT_TRUE(session_->dispatchAsyncLoad(8, 0, 8));
    auto* stalePalette = voxelSim_->simulationGrid().chunkPalette(8, 0, 8);
    ASSERT_NE(stalePalette, nullptr);
    stalePalette->addEntryRaw({0.9f, 0.1f, 0.2f, 0.3f});
    ASSERT_EQ(stalePalette->paletteSize(), 1u);

    auto completions = session_->pollPendingLoads();
    ASSERT_EQ(completions.size(), 1u);
    ASSERT_TRUE(completions[0].success);

    auto* loadedPalette = voxelSim_->simulationGrid().chunkPalette(8, 0, 8);
    ASSERT_NE(loadedPalette, nullptr);
    EXPECT_GT(loadedPalette->paletteSize(), 1u);
    EXPECT_EQ(voxelSim_->activityTracker().getState({8, 0, 8}), recurse::simulation::ChunkState::Active);
}
