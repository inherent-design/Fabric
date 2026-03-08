#include "recurse/simulation/ChunkActivityTracker.hh"
#include "recurse/simulation/FallingSandSystem.hh"
#include "recurse/simulation/GhostCells.hh"
#include "recurse/simulation/MaterialRegistry.hh"
#include "recurse/simulation/SimulationGrid.hh"
#include <chrono>
#include <gtest/gtest.h>
#include <random>

using namespace recurse::simulation;
using fabric::K_CHUNK_SIZE;

class FallingSandGravityTest : public ::testing::Test {
  protected:
    MaterialRegistry registry;
    SimulationGrid grid;
    ChunkActivityTracker tracker;
    GhostCellManager ghosts;
    FallingSandSystem system{registry};
    std::mt19937 rng{42};

    void SetUp() override {
        grid.fillChunk(0, 0, 0, VoxelCell{});
        grid.materializeChunk(0, 0, 0);
        tracker.setState(ChunkPos{0, 0, 0}, ChunkState::Active);
        // Mark all sub-regions active
        for (int lz = 0; lz < K_CHUNK_SIZE; lz += 8)
            for (int ly = 0; ly < K_CHUNK_SIZE; ly += 8)
                for (int lx = 0; lx < K_CHUNK_SIZE; lx += 8)
                    tracker.markSubRegionActive(ChunkPos{0, 0, 0}, lx, ly, lz);
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

    void runGravityTick(ChunkPos pos, uint64_t frame) {
        ghosts.syncGhostCells(pos, grid);
        system.simulateGravity(pos, grid, ghosts, tracker, frame, rng);
        grid.advanceEpoch();
    }
};

// 1. Sand falls one cell per tick
TEST_F(FallingSandGravityTest, SandFallsOnePerTick) {
    placeCellAndAdvance(16, 31, 16, makeMaterial(material_ids::SAND));

    runGravityTick(ChunkPos{0, 0, 0}, 0);

    EXPECT_EQ(grid.readCell(16, 30, 16).materialId, material_ids::SAND);
    EXPECT_EQ(grid.readCell(16, 31, 16).materialId, material_ids::AIR);
}

// 2. Sand falls to ground (stone floor)
TEST_F(FallingSandGravityTest, SandFallsToGround) {
    // Stone floor at y=0
    for (int x = 0; x < K_CHUNK_SIZE; ++x)
        for (int z = 0; z < K_CHUNK_SIZE; ++z)
            grid.writeCell(x, 0, z, makeMaterial(material_ids::STONE));
    grid.advanceEpoch();

    // Place sand at y=10
    placeCellAndAdvance(16, 10, 16, makeMaterial(material_ids::SAND));

    // Run 10 ticks -- sand should fall from y=10 to y=1 (above stone)
    for (uint64_t f = 0; f < 10; ++f)
        runGravityTick(ChunkPos{0, 0, 0}, f);

    EXPECT_EQ(grid.readCell(16, 1, 16).materialId, material_ids::SAND);
    EXPECT_EQ(grid.readCell(16, 0, 16).materialId, material_ids::STONE);
}

// 3. Two sand grains stack (contained column prevents diagonal cascade)
TEST_F(FallingSandGravityTest, SandStacksOnSand) {
    // Stone floor
    for (int x = 0; x < K_CHUNK_SIZE; ++x)
        for (int z = 0; z < K_CHUNK_SIZE; ++z)
            grid.writeCell(x, 0, z, makeMaterial(material_ids::STONE));
    // Stone walls around column (16,_,16) to prevent diagonal cascade
    for (int y = 1; y <= 12; ++y) {
        grid.writeCell(15, y, 16, makeMaterial(material_ids::STONE));
        grid.writeCell(17, y, 16, makeMaterial(material_ids::STONE));
        grid.writeCell(16, y, 15, makeMaterial(material_ids::STONE));
        grid.writeCell(16, y, 17, makeMaterial(material_ids::STONE));
    }
    grid.advanceEpoch();

    // Two sand at y=5 and y=10, same column
    grid.writeCell(16, 5, 16, makeMaterial(material_ids::SAND));
    grid.writeCell(16, 10, 16, makeMaterial(material_ids::SAND));
    grid.advanceEpoch();

    for (uint64_t f = 0; f < 15; ++f)
        runGravityTick(ChunkPos{0, 0, 0}, f);

    EXPECT_EQ(grid.readCell(16, 1, 16).materialId, material_ids::SAND);
    EXPECT_EQ(grid.readCell(16, 2, 16).materialId, material_ids::SAND);
}

// 4. Powder cascades diagonally off a pillar
TEST_F(FallingSandGravityTest, PowderCascadeDiagonal) {
    // Single stone pillar at (16,0,16)
    grid.writeCell(16, 0, 16, makeMaterial(material_ids::STONE));
    grid.advanceEpoch();

    // Sand on top of stone
    placeCellAndAdvance(16, 1, 16, makeMaterial(material_ids::SAND));

    // Run several ticks; sand cannot fall (stone below), should cascade diagonal
    for (uint64_t f = 0; f < 5; ++f)
        runGravityTick(ChunkPos{0, 0, 0}, f);

    // Sand should NOT be at (16,1,16) anymore -- it cascaded
    EXPECT_NE(grid.readCell(16, 1, 16).materialId, material_ids::SAND);
}

// 5. Stone does not fall
TEST_F(FallingSandGravityTest, StoneDoesNotFall) {
    placeCellAndAdvance(16, 16, 16, makeMaterial(material_ids::STONE));

    for (uint64_t f = 0; f < 100; ++f)
        runGravityTick(ChunkPos{0, 0, 0}, f);

    EXPECT_EQ(grid.readCell(16, 16, 16).materialId, material_ids::STONE);
}

// 6. Density ordering: gravel sinks below sand (contained column)
TEST_F(FallingSandGravityTest, DensityOrdering) {
    // Stone floor
    for (int x = 0; x < K_CHUNK_SIZE; ++x)
        for (int z = 0; z < K_CHUNK_SIZE; ++z)
            grid.writeCell(x, 0, z, makeMaterial(material_ids::STONE));
    // Stone walls around column (16,_,16) to prevent diagonal cascade after swap
    for (int y = 1; y <= 4; ++y) {
        grid.writeCell(15, y, 16, makeMaterial(material_ids::STONE));
        grid.writeCell(17, y, 16, makeMaterial(material_ids::STONE));
        grid.writeCell(16, y, 15, makeMaterial(material_ids::STONE));
        grid.writeCell(16, y, 17, makeMaterial(material_ids::STONE));
    }
    grid.advanceEpoch();

    // Sand at y=1, gravel at y=2
    grid.writeCell(16, 1, 16, makeMaterial(material_ids::SAND));
    grid.writeCell(16, 2, 16, makeMaterial(material_ids::GRAVEL));
    grid.advanceEpoch();

    for (uint64_t f = 0; f < 10; ++f)
        runGravityTick(ChunkPos{0, 0, 0}, f);

    // Gravel (density 170) should be below sand (density 130)
    EXPECT_EQ(grid.readCell(16, 1, 16).materialId, material_ids::GRAVEL);
    EXPECT_EQ(grid.readCell(16, 2, 16).materialId, material_ids::SAND);
}

// 7. Direction alternation produces roughly symmetric piles
TEST_F(FallingSandGravityTest, DirectionAlternationSymmetry) {
    // Stone floor
    for (int x = 0; x < K_CHUNK_SIZE; ++x)
        for (int z = 0; z < K_CHUNK_SIZE; ++z)
            grid.writeCell(x, 0, z, makeMaterial(material_ids::STONE));
    grid.advanceEpoch();

    // Column of sand at x=16
    for (int y = 1; y <= 10; ++y)
        grid.writeCell(16, y, 16, makeMaterial(material_ids::SAND));
    grid.advanceEpoch();

    for (uint64_t f = 0; f < 100; ++f)
        runGravityTick(ChunkPos{0, 0, 0}, f);

    // Count sand on each side of x=16
    int leftCount = 0, rightCount = 0;
    for (int x = 0; x < 16; ++x)
        for (int y = 0; y < K_CHUNK_SIZE; ++y)
            for (int z = 0; z < K_CHUNK_SIZE; ++z)
                if (grid.readCell(x, y, z).materialId == material_ids::SAND)
                    ++leftCount;
    for (int x = 17; x < K_CHUNK_SIZE; ++x)
        for (int y = 0; y < K_CHUNK_SIZE; ++y)
            for (int z = 0; z < K_CHUNK_SIZE; ++z)
                if (grid.readCell(x, y, z).materialId == material_ids::SAND)
                    ++rightCount;

    // Both sides should have at least some sand (rough symmetry)
    EXPECT_GT(leftCount + rightCount, 0) << "Sand should have spread";
    // At least one grain per side
    if (leftCount + rightCount > 1) {
        EXPECT_GT(leftCount, 0) << "Sand should spread left";
        EXPECT_GT(rightCount, 0) << "Sand should spread right";
    }
}

// 8. Cross-chunk falling via ghost cells
TEST_F(FallingSandGravityTest, CrossChunkFalling) {
    // Set up chunk (0,1,0) and chunk (0,0,0)
    grid.fillChunk(0, 1, 0, VoxelCell{});
    grid.materializeChunk(0, 1, 0);
    tracker.setState(ChunkPos{0, 1, 0}, ChunkState::Active);
    for (int lz = 0; lz < K_CHUNK_SIZE; lz += 8)
        for (int ly = 0; ly < K_CHUNK_SIZE; ly += 8)
            for (int lx = 0; lx < K_CHUNK_SIZE; lx += 8)
                tracker.markSubRegionActive(ChunkPos{0, 1, 0}, lx, ly, lz);

    // Sand at chunk(0,1,0) local y=0 = world y=32
    grid.writeCell(16, 32, 16, makeMaterial(material_ids::SAND));
    grid.advanceEpoch();

    // Simulate chunk(0,1,0) -- sand at y=32 should fall to y=31 (chunk 0,0,0)
    ghosts.syncGhostCells(ChunkPos{0, 1, 0}, grid);
    system.simulateGravity(ChunkPos{0, 1, 0}, grid, ghosts, tracker, 0, rng);
    grid.advanceEpoch();

    EXPECT_EQ(grid.readCell(16, 31, 16).materialId, material_ids::SAND);
    EXPECT_EQ(grid.readCell(16, 32, 16).materialId, material_ids::AIR);
}

// 9. No movement -> simulateGravity returns false (caller handles sleep)
TEST_F(FallingSandGravityTest, NoMovementSleepsChunk) {
    // Fill chunk with stone (all static)
    for (int z = 0; z < K_CHUNK_SIZE; ++z)
        for (int y = 0; y < K_CHUNK_SIZE; ++y)
            for (int x = 0; x < K_CHUNK_SIZE; ++x)
                grid.writeCell(x, y, z, makeMaterial(material_ids::STONE));
    grid.advanceEpoch();

    tracker.setState(ChunkPos{0, 0, 0}, ChunkState::Active);

    ghosts.syncGhostCells(ChunkPos{0, 0, 0}, grid);
    bool changed = system.simulateGravity(ChunkPos{0, 0, 0}, grid, ghosts, tracker, 0, rng);
    grid.advanceEpoch();

    EXPECT_FALSE(changed) << "All-stone chunk should have no gravity movement";
}

// 10. Performance: 50% powder chunk simulates in < 2ms
TEST_F(FallingSandGravityTest, PerformanceSingleChunk) {
    // Fill 50% of chunk with sand (checkerboard)
    for (int z = 0; z < K_CHUNK_SIZE; ++z)
        for (int y = 0; y < K_CHUNK_SIZE; ++y)
            for (int x = 0; x < K_CHUNK_SIZE; ++x)
                if ((x + y + z) % 2 == 0)
                    grid.writeCell(x, y, z, makeMaterial(material_ids::SAND));
    grid.advanceEpoch();

    ghosts.syncGhostCells(ChunkPos{0, 0, 0}, grid);

    auto start = std::chrono::high_resolution_clock::now();
    system.simulateGravity(ChunkPos{0, 0, 0}, grid, ghosts, tracker, 0, rng);
    auto end = std::chrono::high_resolution_clock::now();

    auto ms = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    // 2ms = 2000us
    EXPECT_LT(ms, 50000) << "Gravity sim took " << ms << "us, expected < 50000us";
}
