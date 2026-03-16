#include "fabric/fx/WorldContext.hh"
#include "recurse/persistence/WorldSession.hh"
#include "recurse/systems/VoxelSimulationSystem.hh"
#include "recurse/world/ChunkOps.hh"

#include "fabric/core/AppContext.hh"
#include "fabric/core/SystemRegistry.hh"
#include "fabric/platform/ConfigManager.hh"
#include "fabric/platform/JobScheduler.hh"
#include "fabric/resource/AssetRegistry.hh"
#include "fabric/resource/ResourceHub.hh"
#include "recurse/persistence/ChunkSaveService.hh"
#include "recurse/simulation/ChunkRegistry.hh"
#include "recurse/simulation/ChunkState.hh"
#include "recurse/simulation/SimulationGrid.hh"
#include "recurse/simulation/VoxelMaterial.hh"

#include <chrono>
#include <filesystem>
#include <gtest/gtest.h>

#include <flecs.h>

namespace fs = std::filesystem;

using namespace recurse::systems;
using recurse::simulation::VoxelCell;
using recurse::simulation::material_ids::STONE;

class SubmitOpsTest : public ::testing::Test {
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

        materializeActiveChunk(0, 0, 0);
        materializeActiveChunk(1, 0, 0);
        materializeActiveChunk(2, 0, 0);

        tmpDir_ = fs::temp_directory_path() /
                  ("fabric_submit_test_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
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
};

TEST_F(SubmitOpsTest, LoadChunk_ReturnsFalseWhenNotInDb) {
    fabric::fx::WorldContext<recurse::WorldSession> ctx(*session_);
    bool result = ctx.submit(recurse::ops::LoadChunk{50, 50, 50});
    EXPECT_FALSE(result);
}

TEST_F(SubmitOpsTest, SaveChunk_MarksDirty) {
    fabric::fx::WorldContext<recurse::WorldSession> ctx(*session_);
    auto* svc = session_->saveService();
    ASSERT_NE(svc, nullptr);
    size_t before = svc->pendingCount();
    ctx.submit(recurse::ops::SaveChunk{0, 0, 0});
    EXPECT_EQ(svc->pendingCount(), before + 1);
}

TEST_F(SubmitOpsTest, PersistChunk_EncodesAndEnqueues) {
    fabric::fx::WorldContext<recurse::WorldSession> ctx(*session_);
    ctx.submit(recurse::ops::PersistChunk{0, 0, 0});
    SUCCEED();
}

TEST_F(SubmitOpsTest, RemoveChunk_RemovesFromGrid) {
    fabric::fx::WorldContext<recurse::WorldSession> ctx(*session_);
    EXPECT_TRUE(ctx.resolve(recurse::ops::HasChunk{2, 0, 0}));
    ctx.submit(recurse::ops::RemoveChunk{2, 0, 0});
    EXPECT_FALSE(ctx.resolve(recurse::ops::HasChunk{2, 0, 0}));
}

TEST_F(SubmitOpsTest, CancelPendingLoad_ReturnsFalseWhenNoPending) {
    fabric::fx::WorldContext<recurse::WorldSession> ctx(*session_);
    bool cancelled = ctx.submit(recurse::ops::CancelPendingLoad{0, 0, 0});
    EXPECT_FALSE(cancelled);
}

TEST_F(SubmitOpsTest, GenerateChunks_NoOpWithoutTerrain) {
    fabric::fx::WorldContext<recurse::WorldSession> ctx(*session_);
    int before = ctx.resolve(recurse::ops::ChunkCount{});
    std::vector<std::tuple<int, int, int>> coords{{10, 10, 10}, {11, 10, 10}};
    ctx.submit(recurse::ops::GenerateChunks{std::move(coords)});
    int after = ctx.resolve(recurse::ops::ChunkCount{});
    EXPECT_EQ(after, before);
}

TEST_F(SubmitOpsTest, Tick_FlushesAndUpdates) {
    fabric::fx::WorldContext<recurse::WorldSession> ctx(*session_);
    ctx.submit(recurse::ops::Tick{0.016f});
    SUCCEED();
}
