#include "recurse/simulation/ChunkActivityTracker.hh"
#include "recurse/simulation/FallingSandSystem.hh"
#include "recurse/simulation/GhostCells.hh"
#include "recurse/simulation/MaterialRegistry.hh"
#include "recurse/simulation/SimulationGrid.hh"
#include "recurse/simulation/VoxelSimulationSystem.hh"
#include <bit>
#include <gtest/gtest.h>
#include <random>

using namespace recurse::simulation;
using fabric::K_CHUNK_SIZE;

class FallingSandPhysicsChangeTest : public ::testing::Test {
  protected:
    MaterialRegistry registry;
    SimulationGrid grid;
    ChunkActivityTracker tracker;
    GhostCellManager ghosts;
    FallingSandSystem system{registry};
    BoundaryWriteQueue boundaryWrites;
    std::vector<CellSwap> cellSwaps;
    std::mt19937 rng{42};

    void SetUp() override {
        grid.fillChunk(0, 0, 0, VoxelCell{});
        grid.materializeChunk(0, 0, 0);
        tracker.setState(ChunkCoord{0, 0, 0}, ChunkState::Active);
        for (int lz = 0; lz < K_CHUNK_SIZE; lz += 8)
            for (int ly = 0; ly < K_CHUNK_SIZE; ly += 8)
                for (int lx = 0; lx < K_CHUNK_SIZE; lx += 8)
                    tracker.markSubRegionActive(ChunkCoord{0, 0, 0}, lx, ly, lz);
    }

    VoxelCell makeMaterial(MaterialId id) {
        VoxelCell c;
        c.materialId = id;
        return c;
    }

    void placeCellAndAdvance(int wx, int wy, int wz, VoxelCell cell) {
        grid.writeCell(wx, wy, wz, cell);
        grid.advanceEpoch();
    }
};

// 1. Sand falling one cell records exactly 2 CellSwap entries
TEST_F(FallingSandPhysicsChangeTest, SandFallRecordsCellSwaps) {
    placeCellAndAdvance(16, 31, 16, makeMaterial(material_ids::SAND));

    ghosts.syncGhostCells(ChunkCoord{0, 0, 0}, grid);
    system.simulateGravity(ChunkCoord{0, 0, 0}, grid, ghosts, tracker, 0, rng, boundaryWrites, cellSwaps);

    // One swap = 2 entries (source cleared, dest filled)
    ASSERT_EQ(cellSwaps.size(), 2u);

    VoxelCell sand = makeMaterial(material_ids::SAND);
    VoxelCell air{};
    uint32_t sandRaw = std::bit_cast<uint32_t>(sand);
    uint32_t airRaw = std::bit_cast<uint32_t>(air);

    // Source entry: sand was at (16,31,16), replaced by air (the cell below)
    const auto& src = cellSwaps[0];
    EXPECT_EQ(src.lx, 16);
    EXPECT_EQ(src.ly, 31);
    EXPECT_EQ(src.lz, 16);
    EXPECT_EQ(src.oldCell, sandRaw);
    EXPECT_EQ(src.newCell, airRaw);
    EXPECT_EQ(src.chunk, (ChunkCoord{0, 0, 0}));

    // Dest entry: air was at (16,30,16), replaced by sand
    const auto& dst = cellSwaps[1];
    EXPECT_EQ(dst.lx, 16);
    EXPECT_EQ(dst.ly, 30);
    EXPECT_EQ(dst.lz, 16);
    EXPECT_EQ(dst.oldCell, airRaw);
    EXPECT_EQ(dst.newCell, sandRaw);
    EXPECT_EQ(dst.chunk, (ChunkCoord{0, 0, 0}));
}

// 2. Liquid horizontal flow records CellSwap entries
TEST_F(FallingSandPhysicsChangeTest, LiquidFlowRecordsCellSwaps) {
    // Stone floor so water does not fall
    for (int x = 0; x < K_CHUNK_SIZE; ++x)
        for (int z = 0; z < K_CHUNK_SIZE; ++z)
            grid.writeCell(x, 0, z, makeMaterial(material_ids::STONE));
    grid.advanceEpoch();

    placeCellAndAdvance(16, 1, 16, makeMaterial(material_ids::WATER));

    ghosts.syncGhostCells(ChunkCoord{0, 0, 0}, grid);
    system.simulateLiquid(ChunkCoord{0, 0, 0}, grid, ghosts, tracker, 0, rng, boundaryWrites, cellSwaps);

    // Water should flow horizontally; at least 2 swap entries (1 movement)
    EXPECT_GE(cellSwaps.size(), 2u);

    // All entries should reference chunk (0,0,0)
    for (const auto& swap : cellSwaps) {
        EXPECT_EQ(swap.chunk, (ChunkCoord{0, 0, 0}));
    }
}

// 3. physicsChanges() populated after tick
TEST_F(FallingSandPhysicsChangeTest, PhysicsChangesPopulatedAfterTick) {
    VoxelSimulationSystem sim;
    sim.scheduler().disableForTesting();
    auto& g = sim.grid();

    g.fillChunk(0, 0, 0, VoxelCell{});
    g.materializeChunk(0, 0, 0);
    sim.activityTracker().setState(ChunkCoord{0, 0, 0}, ChunkState::Active);
    for (int lz = 0; lz < K_CHUNK_SIZE; lz += 8)
        for (int ly = 0; ly < K_CHUNK_SIZE; ly += 8)
            for (int lx = 0; lx < K_CHUNK_SIZE; lx += 8)
                sim.activityTracker().markSubRegionActive(ChunkCoord{0, 0, 0}, lx, ly, lz);

    // Place sand that will fall
    g.writeCell(16, 31, 16, makeMaterial(material_ids::SAND));
    g.advanceEpoch();

    sim.tick();

    const auto& changes = sim.physicsChanges();
    EXPECT_FALSE(changes.empty());

    auto it = changes.find(ChunkCoord{0, 0, 0});
    ASSERT_NE(it, changes.end());
    EXPECT_GE(it->second.size(), 2u);
}

// 4. Settled chunk with only stone has no physics changes
TEST_F(FallingSandPhysicsChangeTest, SettledChunkHasNoPhysicsChanges) {
    VoxelSimulationSystem sim;
    sim.scheduler().disableForTesting();
    auto& g = sim.grid();

    g.fillChunk(0, 0, 0, VoxelCell{});
    g.materializeChunk(0, 0, 0);

    // Fill with stone (static; no movement possible)
    for (int z = 0; z < K_CHUNK_SIZE; ++z)
        for (int y = 0; y < K_CHUNK_SIZE; ++y)
            for (int x = 0; x < K_CHUNK_SIZE; ++x)
                g.writeCell(x, y, z, makeMaterial(material_ids::STONE));
    g.advanceEpoch();

    sim.activityTracker().setState(ChunkCoord{0, 0, 0}, ChunkState::Active);
    for (int lz = 0; lz < K_CHUNK_SIZE; lz += 8)
        for (int ly = 0; ly < K_CHUNK_SIZE; ly += 8)
            for (int lx = 0; lx < K_CHUNK_SIZE; lx += 8)
                sim.activityTracker().markSubRegionActive(ChunkCoord{0, 0, 0}, lx, ly, lz);

    sim.tick();

    const auto& changes = sim.physicsChanges();
    auto it = changes.find(ChunkCoord{0, 0, 0});
    if (it != changes.end()) {
        EXPECT_TRUE(it->second.empty());
    }
    // Chunk should have settled
    EXPECT_EQ(sim.activityTracker().getState(ChunkCoord{0, 0, 0}), ChunkState::Sleeping);
}

// 5. Matter conservation holds with CellSwap tracking
TEST_F(FallingSandPhysicsChangeTest, MatterConservationWithTracking) {
    VoxelSimulationSystem sim;
    sim.scheduler().disableForTesting();
    auto& g = sim.grid();

    g.fillChunk(0, 0, 0, VoxelCell{});
    g.materializeChunk(0, 0, 0);

    // Stone floor
    for (int x = 0; x < K_CHUNK_SIZE; ++x)
        for (int z = 0; z < K_CHUNK_SIZE; ++z)
            g.writeCell(x, 0, z, makeMaterial(material_ids::STONE));
    g.advanceEpoch();

    // Place 10 sand cells
    const int N = 10;
    for (int i = 0; i < N; ++i)
        g.writeCell(14 + i % 5, 5 + i / 5, 16, makeMaterial(material_ids::SAND));
    g.advanceEpoch();

    auto countSand = [&]() {
        int count = 0;
        for (int z = 0; z < K_CHUNK_SIZE; ++z)
            for (int y = 0; y < K_CHUNK_SIZE; ++y)
                for (int x = 0; x < K_CHUNK_SIZE; ++x)
                    if (g.readCell(x, y, z).materialId == material_ids::SAND)
                        ++count;
        return count;
    };

    int initialSand = countSand();
    ASSERT_EQ(initialSand, N);

    for (int i = 0; i < 30; ++i) {
        sim.activityTracker().setState(ChunkCoord{0, 0, 0}, ChunkState::Active);
        for (int lz = 0; lz < K_CHUNK_SIZE; lz += 8)
            for (int ly = 0; ly < K_CHUNK_SIZE; ly += 8)
                for (int lx = 0; lx < K_CHUNK_SIZE; lx += 8)
                    sim.activityTracker().markSubRegionActive(ChunkCoord{0, 0, 0}, lx, ly, lz);
        sim.tick();
    }

    int finalSand = countSand();
    EXPECT_EQ(finalSand, initialSand) << "Sand count must be conserved with CellSwap tracking";
}
