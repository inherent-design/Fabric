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
#include "recurse/simulation/CellAccessors.hh"
#include "recurse/simulation/ChunkRegistry.hh"
#include "recurse/simulation/ChunkState.hh"
#include "recurse/simulation/SimulationGrid.hh"
#include "recurse/simulation/VoxelMaterial.hh"
#include "recurse/world/VoxelOps.hh"

#include <chrono>
#include <filesystem>
#include <gtest/gtest.h>

#include <flecs.h>

namespace fs = std::filesystem;

using namespace recurse::systems;
using recurse::simulation::cellForMaterial;
using recurse::simulation::ChunkSlotState;
using recurse::simulation::VoxelCell;
using recurse::simulation::material_ids::STONE;

class WorldContextTest : public ::testing::Test {
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

        tmpDir_ = fs::temp_directory_path() /
                  ("fabric_ctx_test_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
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
        grid.writeCell(cx * 32 + 4, cy * 32 + 4, cz * 32 + 4, cellForMaterial(STONE));
        grid.syncChunkBuffers(cx, cy, cz);
        transition<Generating, Active>(generating, reg);
    }
};

// ---------------------------------------------------------------------------
// Resolve ops
// ---------------------------------------------------------------------------

TEST_F(WorldContextTest, ResolveHasChunk_True) {
    fabric::fx::WorldContext<recurse::WorldSession> ctx(*session_);
    EXPECT_TRUE(ctx.resolve(recurse::ops::HasChunk{0, 0, 0}));
}

TEST_F(WorldContextTest, ResolveHasChunk_False) {
    fabric::fx::WorldContext<recurse::WorldSession> ctx(*session_);
    EXPECT_FALSE(ctx.resolve(recurse::ops::HasChunk{99, 99, 99}));
}

TEST_F(WorldContextTest, ResolveFindSlot_Exists) {
    fabric::fx::WorldContext<recurse::WorldSession> ctx(*session_);
    auto* slot = ctx.resolve(recurse::ops::FindSlot{0, 0, 0});
    ASSERT_NE(slot, nullptr);
    EXPECT_EQ(slot->state, ChunkSlotState::Active);
}

TEST_F(WorldContextTest, ResolveFindSlot_Missing) {
    fabric::fx::WorldContext<recurse::WorldSession> ctx(*session_);
    auto* slot = ctx.resolve(recurse::ops::FindSlot{99, 99, 99});
    EXPECT_EQ(slot, nullptr);
}

TEST_F(WorldContextTest, ResolveIsInSavedRegion_Empty) {
    fabric::fx::WorldContext<recurse::WorldSession> ctx(*session_);
    EXPECT_FALSE(ctx.resolve(recurse::ops::IsInSavedRegion{0, 0, 0}));
}

TEST_F(WorldContextTest, ResolveHasPendingLoad_NoPending) {
    fabric::fx::WorldContext<recurse::WorldSession> ctx(*session_);
    EXPECT_FALSE(ctx.resolve(recurse::ops::HasPendingLoad{0, 0, 0}));
}

TEST_F(WorldContextTest, ResolveHasPersistPending_TrueAfterPersistChunk) {
    fabric::fx::WorldContext<recurse::WorldSession> ctx(*session_);
    EXPECT_FALSE(ctx.resolve(recurse::ops::HasPersistPending{0, 0, 0}));

    ctx.submit(recurse::ops::PersistChunk{0, 0, 0});

    EXPECT_TRUE(ctx.resolve(recurse::ops::HasPersistPending{0, 0, 0}));
}

TEST_F(WorldContextTest, ResolveQueryChunkEntities_Empty) {
    fabric::fx::WorldContext<recurse::WorldSession> ctx(*session_);
    fabric::ChunkCoord coord{0, 0, 0};
    EXPECT_FALSE(ctx.resolve(recurse::ops::QueryChunkEntities{coord}));
}

TEST_F(WorldContextTest, ResolveQueryChunkEntities_Present) {
    fabric::fx::WorldContext<recurse::WorldSession> ctx(*session_);
    fabric::ChunkCoord coord{0, 0, 0};
    session_->chunkEntities()[coord] = world.get().entity();
    EXPECT_TRUE(ctx.resolve(recurse::ops::QueryChunkEntities{coord}));
}

TEST_F(WorldContextTest, ResolveReadBuffer_Active) {
    fabric::fx::WorldContext<recurse::WorldSession> ctx(*session_);
    const auto* buf = ctx.resolve(recurse::ops::ReadBuffer{0, 0, 0});
    ASSERT_NE(buf, nullptr);
}

TEST_F(WorldContextTest, ResolveReadBuffer_Missing) {
    fabric::fx::WorldContext<recurse::WorldSession> ctx(*session_);
    const auto* buf = ctx.resolve(recurse::ops::ReadBuffer{99, 99, 99});
    EXPECT_EQ(buf, nullptr);
}

TEST_F(WorldContextTest, ResolveWriteBuffer_Active) {
    fabric::fx::WorldContext<recurse::WorldSession> ctx(*session_);
    auto* buf = ctx.resolve(recurse::ops::WriteBuffer{0, 0, 0});
    ASSERT_NE(buf, nullptr);
}

TEST_F(WorldContextTest, ResolveChunkCount) {
    fabric::fx::WorldContext<recurse::WorldSession> ctx(*session_);
    EXPECT_EQ(ctx.resolve(recurse::ops::ChunkCount{}), 2);
}

TEST_F(WorldContextTest, ResolveActiveChunkCount) {
    fabric::fx::WorldContext<recurse::WorldSession> ctx(*session_);
    int count = ctx.resolve(recurse::ops::ActiveChunkCount{});
    EXPECT_GE(count, 0);
}

TEST_F(WorldContextTest, ResolvePollPendingLoads_Empty) {
    fabric::fx::WorldContext<recurse::WorldSession> ctx(*session_);
    auto completions = ctx.resolve(recurse::ops::PollPendingLoads{16});
    EXPECT_TRUE(completions.empty());
}

TEST_F(WorldContextTest, ResolveQueryLODChunks_Empty) {
    fabric::fx::WorldContext<recurse::WorldSession> ctx(*session_);
    EXPECT_EQ(ctx.resolve(recurse::ops::QueryLODChunks{}), 0);
}

// ---------------------------------------------------------------------------
// Submit ops
// ---------------------------------------------------------------------------

TEST_F(WorldContextTest, SubmitLoadChunk_NoDbEntry) {
    fabric::fx::WorldContext<recurse::WorldSession> ctx(*session_);
    bool dispatched = ctx.submit(recurse::ops::LoadChunk{50, 50, 50});
    EXPECT_FALSE(dispatched);
}

TEST_F(WorldContextTest, SubmitSaveChunk_NoCrash) {
    fabric::fx::WorldContext<recurse::WorldSession> ctx(*session_);
    ctx.submit(recurse::ops::SaveChunk{0, 0, 0});
    SUCCEED();
}

TEST_F(WorldContextTest, SubmitPersistChunk_NoCrash) {
    fabric::fx::WorldContext<recurse::WorldSession> ctx(*session_);
    ctx.submit(recurse::ops::PersistChunk{0, 0, 0});
    SUCCEED();
}

TEST_F(WorldContextTest, SubmitRemoveChunk_DecreasesCount) {
    fabric::fx::WorldContext<recurse::WorldSession> ctx(*session_);
    int before = ctx.resolve(recurse::ops::ChunkCount{});
    ctx.submit(recurse::ops::RemoveChunk{1, 0, 0});
    int after = ctx.resolve(recurse::ops::ChunkCount{});
    EXPECT_EQ(after, before - 1);
}

TEST_F(WorldContextTest, SubmitCancelPendingLoad_NoPending) {
    fabric::fx::WorldContext<recurse::WorldSession> ctx(*session_);
    bool cancelled = ctx.submit(recurse::ops::CancelPendingLoad{0, 0, 0});
    EXPECT_FALSE(cancelled);
}

TEST_F(WorldContextTest, SubmitGenerateChunks_NoOpWithoutTerrain) {
    fabric::fx::WorldContext<recurse::WorldSession> ctx(*session_);
    int before = ctx.resolve(recurse::ops::ChunkCount{});
    std::vector<std::tuple<int, int, int>> coords{{5, 5, 5}};
    ctx.submit(recurse::ops::GenerateChunks{std::move(coords)});
    int after = ctx.resolve(recurse::ops::ChunkCount{});
    EXPECT_EQ(after, before);
}

TEST_F(WorldContextTest, SubmitTick_NoCrash) {
    fabric::fx::WorldContext<recurse::WorldSession> ctx(*session_);
    ctx.submit(recurse::ops::Tick{0.016f});
    SUCCEED();
}

TEST_F(WorldContextTest, SubmitUpdateLODRing_NoCrash) {
    fabric::fx::WorldContext<recurse::WorldSession> ctx(*session_);
    ctx.submit(recurse::ops::UpdateLODRing{0.0f, 64.0f, 0.0f, 4, 8, 4});
    SUCCEED();
}

TEST_F(WorldContextTest, SessionEscapeHatch) {
    fabric::fx::WorldContext<recurse::WorldSession> ctx(*session_);
    auto& entities = ctx.session().chunkEntities();
    EXPECT_TRUE(entities.empty());
}

TEST(WorldOpContractTest, VoxelWriteExposesPointMutationContract) {
    using recurse::FunctionCapability;
    using recurse::FunctionCostClass;
    using recurse::FunctionHistoryMode;
    using recurse::FunctionTargetKind;

    EXPECT_EQ(recurse::ops::VoxelWrite::K_CONTRACT.targetKind, FunctionTargetKind::Voxel);
    EXPECT_EQ(recurse::ops::VoxelWrite::K_CONTRACT.historyMode, FunctionHistoryMode::PerVoxelDelta);
    EXPECT_EQ(recurse::ops::VoxelWrite::K_CONTRACT.costClass, FunctionCostClass::Constant);
    EXPECT_TRUE(
        recurse::hasCapability(recurse::ops::VoxelWrite::K_CONTRACT.capabilities, FunctionCapability::PointWrite));
    EXPECT_TRUE(recurse::hasCapability(recurse::ops::VoxelWrite::K_CONTRACT.capabilities,
                                       FunctionCapability::WakeAffectedChunks));
    EXPECT_TRUE(recurse::hasCapability(recurse::ops::VoxelWrite::K_CONTRACT.capabilities,
                                       FunctionCapability::EmitDetailedHistory));
}

TEST(WorldOpContractTest, ChunkAndRegionOpsExposeContractTargets) {
    using recurse::FunctionCapability;
    using recurse::FunctionCostClass;
    using recurse::FunctionHistoryMode;
    using recurse::FunctionTargetKind;

    EXPECT_EQ(recurse::ops::ReadBuffer::K_CONTRACT.targetKind, FunctionTargetKind::Chunk);
    EXPECT_TRUE(
        recurse::hasCapability(recurse::ops::ReadBuffer::K_CONTRACT.capabilities, FunctionCapability::RegionRead));
    EXPECT_TRUE(recurse::hasCapability(recurse::ops::ReadBuffer::K_CONTRACT.capabilities,
                                       FunctionCapability::RequireMaterializedChunks));
    EXPECT_EQ(recurse::ops::GenerateChunks::K_CONTRACT.targetKind, FunctionTargetKind::Region);
    EXPECT_EQ(recurse::ops::GenerateChunks::K_CONTRACT.historyMode, FunctionHistoryMode::SnapshotOnly);
    EXPECT_EQ(recurse::ops::GenerateChunks::K_CONTRACT.costClass, FunctionCostClass::ChunkLinear);
    EXPECT_TRUE(
        recurse::hasCapability(recurse::ops::GenerateChunks::K_CONTRACT.capabilities, FunctionCapability::RegionWrite));
    EXPECT_TRUE(recurse::hasCapability(recurse::ops::GenerateChunks::K_CONTRACT.capabilities,
                                       FunctionCapability::AllowChunkStreaming));
    EXPECT_TRUE(recurse::hasCapability(recurse::ops::GenerateChunks::K_CONTRACT.capabilities,
                                       FunctionCapability::EmitSummaryHistory));
    EXPECT_TRUE(recurse::ops::GenerateChunks::K_CONTRACT.needsBudget());
}
