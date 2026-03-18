#include "recurse/persistence/ReplayExecutor.hh"
#include "recurse/persistence/FchkCodec.hh"
#include "recurse/persistence/WorldTransactionStore.hh"
#include "recurse/simulation/ChunkActivityTracker.hh"
#include "recurse/simulation/FallingSandSystem.hh"
#include "recurse/simulation/GhostCells.hh"
#include "recurse/simulation/MaterialRegistry.hh"
#include "recurse/simulation/SimulationGrid.hh"
#include "recurse/simulation/VoxelSimulationSystem.hh"
#include <cstring>
#include <gtest/gtest.h>
#include <random>

using namespace recurse;
using namespace recurse::simulation;
using namespace recurse::persistence;

namespace {

class StubTransactionStore : public WorldTransactionStore {
  public:
    std::vector<VoxelChange> stubbedChanges;

    void logChanges(std::span<const VoxelChange>) override {}
    std::vector<VoxelChange> queryChanges(const ChangeQuery&) override { return stubbedChanges; }
    int64_t countChanges(const ChangeQuery&) override { return 0; }
    void saveSnapshot(int, int, int, const ChunkBlob&) override {}
    std::optional<ChunkBlob> loadSnapshot(int, int, int, int64_t) override { return std::nullopt; }
    void prune(int64_t, int64_t) override {}
    void flush() override {}
};

ChunkBlob encodeChunk(SimulationGrid& grid, int cx, int cy, int cz) {
    const auto* buf = grid.readBuffer(cx, cy, cz);
    if (!buf)
        return ChunkBlob{};
    return FchkCodec::encode(buf->data(), K_CHUNK_VOLUME * sizeof(VoxelCell));
}

SnapshotSet makeSnapshot(SimulationGrid& grid, const std::vector<fabric::ChunkCoord>& coords, int64_t timeMs) {
    SnapshotSet ss;
    ss.timeMs = timeMs;
    for (const auto& c : coords) {
        ss.chunks.push_back({c, encodeChunk(grid, c.x, c.y, c.z)});
    }
    return ss;
}

} // namespace

class ReplayExecutorTest : public ::testing::Test {
  protected:
    MaterialRegistry registry;
    SimulationGrid grid;
    ChunkActivityTracker tracker;
    GhostCellManager ghosts;
    FallingSandSystem sandSystem{registry};
    StubTransactionStore txStore;
    int64_t worldSeed = 12345;

    void SetUp() override {
        grid.fillChunk(0, 0, 0, VoxelCell{});
        grid.materializeChunk(0, 0, 0);
        tracker.setState(ChunkCoord{0, 0, 0}, ChunkState::Active);
        activateAllSubRegions(ChunkCoord{0, 0, 0});
    }

    void activateAllSubRegions(ChunkCoord pos) {
        for (int lz = 0; lz < K_CHUNK_SIZE; lz += K_PHYS_TILE_SIZE)
            for (int ly = 0; ly < K_CHUNK_SIZE; ly += K_PHYS_TILE_SIZE)
                for (int lx = 0; lx < K_CHUNK_SIZE; lx += K_PHYS_TILE_SIZE)
                    tracker.markSubRegionActive(pos, lx, ly, lz);
    }

    VoxelCell makeMaterial(MaterialId id) {
        VoxelCell c{};
        c.materialId = id;
        return c;
    }

    ReplayExecutor makeExecutor() {
        return ReplayExecutor(txStore, grid, sandSystem, ghosts, tracker, worldSeed, &registry);
    }

    void runProductionTicks(uint64_t ticks) {
        for (uint64_t t = 0; t < ticks; ++t) {
            auto collected = tracker.collectActiveChunks();
            std::vector<ActiveChunkEntry> active;
            for (const auto& e : collected)
                if (tracker.getState(e.pos) == ChunkState::Active)
                    active.push_back(e);
            if (active.empty())
                break;

            std::sort(active.begin(), active.end(),
                      [](const ActiveChunkEntry& a, const ActiveChunkEntry& b) { return a.pos < b.pos; });

            grid.registry().resolveBufferPointers(grid.currentEpoch());
            std::vector<ChunkCoord> positions;
            for (const auto& e : active)
                positions.push_back(e.pos);
            ghosts.syncAll(positions, grid);

            for (const auto& e : active) {
                uint64_t hash = spatialHash(e.pos);
                std::mt19937 rng(static_cast<uint32_t>(worldSeed ^ hash));
                bool reverseDir = (hash & 1) != 0;
                BoundaryWriteQueue bw;
                std::vector<CellSwap> cs;
                sandSystem.simulateChunk(e.pos, grid, ghosts, tracker, reverseDir, rng, bw, cs);
            }
            grid.advanceEpoch();
        }
    }
};

TEST_F(ReplayExecutorTest, EmptySnapshotReturnsSnapshotMissing) {
    SnapshotSet empty{};
    empty.timeMs = 0;

    auto executor = makeExecutor();
    auto result = executor.replayDelta(empty, {}, 10);
    EXPECT_EQ(result.status, ReplayStatus::SnapshotMissing);
    EXPECT_EQ(result.ticksReplayed, 0u);
}

TEST_F(ReplayExecutorTest, SnapshotRoundTrip) {
    grid.writeCell(10, 10, 10, makeMaterial(material_ids::STONE));
    grid.writeCell(15, 15, 15, makeMaterial(material_ids::SAND));
    grid.advanceEpoch();

    auto snapshot = makeSnapshot(grid, {ChunkCoord{0, 0, 0}}, 1000);
    std::array<VoxelCell, K_CHUNK_VOLUME> original = *grid.readBuffer(0, 0, 0);

    grid.clear();
    tracker.clear();
    grid.fillChunk(0, 0, 0, VoxelCell{});
    grid.materializeChunk(0, 0, 0);

    auto executor = makeExecutor();
    auto result = executor.replayDelta(snapshot, {}, 0);
    ASSERT_EQ(result.status, ReplayStatus::Ok);
    EXPECT_EQ(result.ticksReplayed, 0u);

    const auto* buf = grid.readBuffer(0, 0, 0);
    ASSERT_NE(buf, nullptr);
    for (int i = 0; i < K_CHUNK_VOLUME; ++i) {
        EXPECT_EQ((*buf)[i].materialId, original[i].materialId) << "Mismatch at cell " << i;
    }
}

TEST_F(ReplayExecutorTest, DeterministicReplayMatchesProduction) {
    grid.writeCell(16, 20, 16, makeMaterial(material_ids::SAND));
    grid.advanceEpoch();

    auto snapshot = makeSnapshot(grid, {ChunkCoord{0, 0, 0}}, 0);

    constexpr uint64_t N = 10;
    runProductionTicks(N);

    std::array<VoxelCell, K_CHUNK_VOLUME> productionState;
    const auto* buf = grid.readBuffer(0, 0, 0);
    ASSERT_NE(buf, nullptr);
    productionState = *buf;

    grid.clear();
    tracker.clear();
    ghosts.clear();
    grid.fillChunk(0, 0, 0, VoxelCell{});
    grid.materializeChunk(0, 0, 0);

    auto executor = makeExecutor();
    auto result = executor.replayDelta(snapshot, {}, N);
    ASSERT_EQ(result.status, ReplayStatus::Ok);
    EXPECT_EQ(result.ticksReplayed, N);

    const auto* replayBuf = grid.readBuffer(0, 0, 0);
    ASSERT_NE(replayBuf, nullptr);
    for (int i = 0; i < K_CHUNK_VOLUME; ++i) {
        EXPECT_EQ((*replayBuf)[i].materialId, productionState[i].materialId) << "Mismatch at cell index " << i;
    }
}

TEST_F(ReplayExecutorTest, UserEditsInterleaveCorrectly) {
    auto snapshot = makeSnapshot(grid, {ChunkCoord{0, 0, 0}}, 0);

    VoxelChange edit{};
    edit.timestamp = static_cast<int64_t>(3 * (1000.0 / 60.0));
    edit.addr = {0, 0, 0, 16, 10, 16};
    VoxelCell stone = makeMaterial(material_ids::STONE);
    std::memcpy(&edit.newCell, &stone, sizeof(uint32_t));
    edit.source = ChangeSource::Place;

    std::vector<VoxelChange> edits{edit};

    grid.clear();
    tracker.clear();
    grid.fillChunk(0, 0, 0, VoxelCell{});
    grid.materializeChunk(0, 0, 0);

    auto executor = makeExecutor();
    auto result = executor.replayDelta(snapshot, edits, 5);
    ASSERT_EQ(result.status, ReplayStatus::Ok);

    VoxelCell cell = grid.readCell(16, 10, 16);
    EXPECT_EQ(cell.materialId, material_ids::STONE);
}

TEST_F(ReplayExecutorTest, VisualReplayFrameDelivery) {
    grid.writeCell(16, 20, 16, makeMaterial(material_ids::SAND));
    grid.advanceEpoch();
    auto snapshot = makeSnapshot(grid, {ChunkCoord{0, 0, 0}}, 0);

    grid.clear();
    tracker.clear();
    grid.fillChunk(0, 0, 0, VoxelCell{});
    grid.materializeChunk(0, 0, 0);

    std::vector<uint64_t> receivedTicks;
    ReplayObserver observer = [&](const ReplayFrame& frame) -> bool {
        receivedTicks.push_back(frame.tick);
        return true;
    };

    ReplayConfig config{};
    config.headless = false;
    config.speed = 1.0f;

    auto executor = makeExecutor();
    constexpr uint64_t N = 5;
    auto result = executor.replayDelta(snapshot, {}, N, config, observer);

    ASSERT_EQ(result.status, ReplayStatus::Ok);
    EXPECT_EQ(receivedTicks.size(), N);
    for (uint64_t i = 0; i < receivedTicks.size(); ++i) {
        EXPECT_EQ(receivedTicks[i], i);
    }
}

TEST_F(ReplayExecutorTest, ObserverAbortStopsReplay) {
    grid.writeCell(16, 20, 16, makeMaterial(material_ids::SAND));
    grid.advanceEpoch();
    auto snapshot = makeSnapshot(grid, {ChunkCoord{0, 0, 0}}, 0);

    grid.clear();
    tracker.clear();
    grid.fillChunk(0, 0, 0, VoxelCell{});
    grid.materializeChunk(0, 0, 0);

    constexpr uint64_t ABORT_AT = 3;
    ReplayObserver observer = [&](const ReplayFrame& frame) -> bool {
        return frame.tick < ABORT_AT;
    };

    ReplayConfig config{};
    config.headless = false;
    config.speed = 1.0f;

    auto executor = makeExecutor();
    auto result = executor.replayDelta(snapshot, {}, 10, config, observer);

    EXPECT_EQ(result.status, ReplayStatus::Aborted);
    EXPECT_EQ(result.ticksReplayed, ABORT_AT + 1);
}

TEST_F(ReplayExecutorTest, BoundaryDrainDeterministic) {
    grid.fillChunk(1, 0, 0, VoxelCell{});
    grid.materializeChunk(1, 0, 0);
    tracker.setState(ChunkCoord{1, 0, 0}, ChunkState::Active);
    activateAllSubRegions(ChunkCoord{1, 0, 0});

    grid.writeCell(31, 5, 16, makeMaterial(material_ids::SAND));
    grid.advanceEpoch();

    auto snapshot = makeSnapshot(grid, {ChunkCoord{0, 0, 0}, ChunkCoord{1, 0, 0}}, 0);

    auto runOnce = [&]() {
        grid.clear();
        tracker.clear();
        ghosts.clear();
        grid.fillChunk(0, 0, 0, VoxelCell{});
        grid.fillChunk(1, 0, 0, VoxelCell{});
        grid.materializeChunk(0, 0, 0);
        grid.materializeChunk(1, 0, 0);
        auto executor = makeExecutor();
        return executor.replayDelta(snapshot, {}, 10);
    };

    auto r1 = runOnce();
    ASSERT_EQ(r1.status, ReplayStatus::Ok);
    std::array<VoxelCell, K_CHUNK_VOLUME> state0;
    std::array<VoxelCell, K_CHUNK_VOLUME> state1;
    std::memcpy(state0.data(), grid.readBuffer(0, 0, 0)->data(), K_CHUNK_VOLUME * sizeof(VoxelCell));
    std::memcpy(state1.data(), grid.readBuffer(1, 0, 0)->data(), K_CHUNK_VOLUME * sizeof(VoxelCell));

    auto r2 = runOnce();
    ASSERT_EQ(r2.status, ReplayStatus::Ok);
    const auto* buf0 = grid.readBuffer(0, 0, 0);
    const auto* buf1 = grid.readBuffer(1, 0, 0);
    for (int i = 0; i < K_CHUNK_VOLUME; ++i) {
        EXPECT_EQ((*buf0)[i].materialId, state0[i].materialId) << "Non-determinism in chunk (0,0,0) at cell " << i;
        EXPECT_EQ((*buf1)[i].materialId, state1[i].materialId) << "Non-determinism in chunk (1,0,0) at cell " << i;
    }
}

TEST_F(ReplayExecutorTest, SnapshotRestoreRebuildsPaletteFromCellsWhenBlobHasNoPalette) {
    grid.writeCell(6, 6, 6, makeMaterial(material_ids::STONE));
    grid.advanceEpoch();
    auto snapshot = makeSnapshot(grid, {ChunkCoord{0, 0, 0}}, 0);

    grid.clear();
    tracker.clear();
    ghosts.clear();
    grid.fillChunk(0, 0, 0, VoxelCell{});
    grid.materializeChunk(0, 0, 0);
    auto* stalePalette = grid.chunkPalette(0, 0, 0);
    ASSERT_NE(stalePalette, nullptr);
    stalePalette->addEntryRaw({0.9f, 0.1f, 0.2f, 0.3f});
    ASSERT_EQ(stalePalette->paletteSize(), 1u);

    auto executor = makeExecutor();
    auto result = executor.replayDelta(snapshot, {}, 0);

    ASSERT_EQ(result.status, ReplayStatus::Ok);
    auto* restoredPalette = grid.chunkPalette(0, 0, 0);
    ASSERT_NE(restoredPalette, nullptr);
    EXPECT_GT(restoredPalette->paletteSize(), 1u);
    EXPECT_EQ(tracker.getState({0, 0, 0}), ChunkState::Active);
    EXPECT_NE(tracker.getSubRegionMask({0, 0, 0}), 0u);
}
