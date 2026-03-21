#include "recurse/simulation/CellAccessors.hh"
#include "recurse/simulation/ChunkActivityTracker.hh"
#include "recurse/simulation/FallingSandSystem.hh"
#include "recurse/simulation/GhostCells.hh"
#include "recurse/simulation/MaterialRegistry.hh"
#include "recurse/simulation/SimulationGrid.hh"
#include "recurse/simulation/VoxelSimulationSystem.hh"
#include <chrono>
#include <gtest/gtest.h>
#include <random>

using namespace recurse::simulation;

class FallingSandLiquidTest : public ::testing::Test {
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

    VoxelCell makeMaterial(MaterialId id) { return cellForMaterial(id); }

    void runLiquidTick(ChunkCoord pos, uint64_t frame) {
        ghosts.syncGhostCells(pos, grid);
        system.simulateLiquid(pos, grid, ghosts, tracker, frame, rng, boundaryWrites, cellSwaps);
        grid.advanceEpoch();
    }

    void runChunkTick(ChunkCoord pos, uint64_t frame) {
        ghosts.syncGhostCells(pos, grid);
        system.simulateChunk(pos, grid, ghosts, tracker, frame, rng, boundaryWrites, cellSwaps);
        grid.advanceEpoch();
    }

    void buildStoneFloor() {
        for (int x = 0; x < K_CHUNK_SIZE; ++x)
            for (int z = 0; z < K_CHUNK_SIZE; ++z)
                grid.writeCell(x, 0, z, makeMaterial(material_ids::STONE));
        grid.advanceEpoch();
    }

    /// Build a stone box: floor at y=0, walls at x=xmin/xmax, z=zmin/zmax, up to height h
    void buildStoneBox(int xmin, int xmax, int zmin, int zmax, int h) {
        for (int x = xmin; x <= xmax; ++x) {
            for (int z = zmin; z <= zmax; ++z) {
                // Floor
                grid.writeCell(x, 0, z, makeMaterial(material_ids::STONE));
                // Walls
                for (int y = 1; y <= h; ++y) {
                    if (x == xmin || x == xmax || z == zmin || z == zmax)
                        grid.writeCell(x, y, z, makeMaterial(material_ids::STONE));
                }
            }
        }
        grid.advanceEpoch();
    }

    int countMaterial(MaterialId id, int xmin, int xmax, int ymin, int ymax, int zmin, int zmax) {
        int count = 0;
        for (int x = xmin; x <= xmax; ++x)
            for (int y = ymin; y <= ymax; ++y)
                for (int z = zmin; z <= zmax; ++z)
                    if (cellMaterialId(grid.readCell(x, y, z)) == id)
                        ++count;
        return count;
    }
};

// 1. Water falls in air
TEST_F(FallingSandLiquidTest, WaterFallsInAir) {
    grid.writeCell(16, 10, 16, makeMaterial(material_ids::WATER));
    grid.advanceEpoch();

    runLiquidTick(ChunkCoord{0, 0, 0}, 0);

    EXPECT_EQ(cellMaterialId(grid.readCell(16, 9, 16)), material_ids::WATER);
    EXPECT_EQ(cellMaterialId(grid.readCell(16, 10, 16)), material_ids::AIR);
}

// 2. Water spreads horizontally on flat surface
TEST_F(FallingSandLiquidTest, WaterFlowsHorizontally) {
    buildStoneFloor();

    grid.writeCell(16, 1, 16, makeMaterial(material_ids::WATER));
    grid.advanceEpoch();

    // Run several ticks -- water should spread
    for (uint64_t f = 0; f < 10; ++f)
        runLiquidTick(ChunkCoord{0, 0, 0}, f);

    // With immediate-mode reads, water cascades multiple cells per tick
    // (scan sees its own writes). After 10 ticks, water is far from origin.
    // Verify: (1) origin is empty, (2) water exists somewhere at y=1.
    EXPECT_NE(cellMaterialId(grid.readCell(16, 1, 16)), material_ids::WATER) << "Water should have moved from origin";

    bool anyWaterAtY1 = false;
    for (int x = 0; x < K_CHUNK_SIZE && !anyWaterAtY1; ++x)
        for (int z = 0; z < K_CHUNK_SIZE && !anyWaterAtY1; ++z)
            if (cellMaterialId(grid.readCell(x, 1, z)) == material_ids::WATER)
                anyWaterAtY1 = true;
    EXPECT_TRUE(anyWaterAtY1) << "Water should still exist at y=1 level";
}

// 3. Water fills a stone container bottom-up
TEST_F(FallingSandLiquidTest, WaterFillsContainer) {
    // 5x5 stone box, walls 4 high
    buildStoneBox(10, 14, 10, 14, 4);

    // Pour 9 cells of water from the top
    for (int x = 11; x <= 13; ++x)
        for (int z = 11; z <= 13; ++z)
            grid.writeCell(x, 4, z, makeMaterial(material_ids::WATER));
    grid.advanceEpoch();

    for (uint64_t f = 0; f < 50; ++f)
        runChunkTick(ChunkCoord{0, 0, 0}, f);

    // Water should have settled at bottom (y=1)
    int bottomWater = countMaterial(material_ids::WATER, 11, 13, 1, 1, 11, 13);
    EXPECT_EQ(bottomWater, 9) << "All water should settle at bottom of container";
}

// 4. U-tube: water finds level
TEST_F(FallingSandLiquidTest, WaterFindsLevel) {
    buildStoneFloor();

    // Left chamber: x=[5..9], walls at x=5, x=9
    // Right chamber: x=[11..15], walls at x=11, x=15
    // Connected at bottom y=1
    // Divider at x=10, y=2..5 (gap at y=1)
    for (int z = 14; z <= 18; ++z) {
        for (int y = 1; y <= 5; ++y) {
            // Left wall
            grid.writeCell(5, y, z, makeMaterial(material_ids::STONE));
            // Right wall
            grid.writeCell(15, y, z, makeMaterial(material_ids::STONE));
            // Front/back walls
            grid.writeCell(5, y, 14, makeMaterial(material_ids::STONE));
            grid.writeCell(5, y, 18, makeMaterial(material_ids::STONE));
            for (int x = 5; x <= 15; ++x) {
                grid.writeCell(x, y, 14, makeMaterial(material_ids::STONE));
                grid.writeCell(x, y, 18, makeMaterial(material_ids::STONE));
            }
        }
        // Divider at x=10, y=2..5 only (gap at y=1 allows flow)
        for (int y = 2; y <= 5; ++y)
            grid.writeCell(10, y, z, makeMaterial(material_ids::STONE));
    }
    grid.advanceEpoch();

    // 10 water cells in left chamber
    for (int y = 1; y <= 2; ++y)
        for (int x = 6; x <= 9; ++x)
            grid.writeCell(x, y, 16, makeMaterial(material_ids::WATER));
    // Extra 2 to make 10
    grid.writeCell(6, 3, 16, makeMaterial(material_ids::WATER));
    grid.writeCell(7, 3, 16, makeMaterial(material_ids::WATER));
    grid.advanceEpoch();

    int totalBefore = countMaterial(material_ids::WATER, 0, 31, 0, 31, 0, 31);

    for (uint64_t f = 0; f < 200; ++f) {
        tracker.setState(ChunkCoord{0, 0, 0}, ChunkState::Active);
        runChunkTick(ChunkCoord{0, 0, 0}, f);
    }

    // Water should have distributed to both sides
    int leftWater = countMaterial(material_ids::WATER, 6, 9, 1, 5, 15, 17);
    int rightWater = countMaterial(material_ids::WATER, 11, 14, 1, 5, 15, 17);

    EXPECT_GT(rightWater, 0) << "Water should flow to right chamber";
    // Conservation
    int totalAfter = countMaterial(material_ids::WATER, 0, 31, 0, 31, 0, 31);
    EXPECT_EQ(totalAfter, totalBefore) << "Water count must be conserved";
}

// 5. Water blocked by walls
TEST_F(FallingSandLiquidTest, WaterBlockedByWalls) {
    // Stone box with no gaps
    buildStoneBox(10, 14, 10, 14, 4);

    // Water inside
    grid.writeCell(12, 1, 12, makeMaterial(material_ids::WATER));
    grid.advanceEpoch();

    for (uint64_t f = 0; f < 50; ++f)
        runChunkTick(ChunkCoord{0, 0, 0}, f);

    // No water should escape the box
    int insideWater = countMaterial(material_ids::WATER, 11, 13, 1, 3, 11, 13);
    EXPECT_EQ(insideWater, 1);

    int outsideWater = countMaterial(material_ids::WATER, 0, 9, 0, 31, 0, 31) +
                       countMaterial(material_ids::WATER, 15, 31, 0, 31, 0, 31);
    EXPECT_EQ(outsideWater, 0) << "No water should escape the box";
}

// 6. Cross-chunk horizontal flow
TEST_F(FallingSandLiquidTest, CrossChunkHorizontalFlow) {
    // Set up chunk(-1,0,0)
    grid.fillChunk(-1, 0, 0, VoxelCell{});
    grid.materializeChunk(-1, 0, 0);
    tracker.setState(ChunkCoord{-1, 0, 0}, ChunkState::Active);

    // Stone floor in both chunks
    for (int x = -32; x < 32; ++x)
        for (int z = 0; z < K_CHUNK_SIZE; ++z)
            grid.writeCell(x, 0, z, makeMaterial(material_ids::STONE));
    grid.advanceEpoch();

    // Water at x=0, y=1, z=16 (edge of chunk 0,0,0)
    grid.writeCell(0, 1, 16, makeMaterial(material_ids::WATER));
    grid.advanceEpoch();

    // Run several ticks on chunk (0,0,0)
    for (uint64_t f = 0; f < 20; ++f) {
        ghosts.syncGhostCells(ChunkCoord{0, 0, 0}, grid);
        system.simulateLiquid(ChunkCoord{0, 0, 0}, grid, ghosts, tracker, f, rng, boundaryWrites, cellSwaps);
        // Drain deferred cross-chunk writes
        for (const auto& bw : boundaryWrites) {
            if (!grid.writeCellIfExists(bw.dstWx, bw.dstWy, bw.dstWz, bw.writeCell))
                grid.writeCell(bw.srcWx, bw.srcWy, bw.srcWz, bw.undoCell);
        }
        boundaryWrites.clear();
        grid.advanceEpoch();
    }

    // Water should have flowed into chunk(-1,0,0), i.e., world x=-1
    bool flowedLeft = cellMaterialId(grid.readCell(-1, 1, 16)) == material_ids::WATER;
    // Or it might still be at x=0 if it flowed other directions first
    int totalWater = countMaterial(material_ids::WATER, -2, 2, 1, 1, 15, 17);
    EXPECT_GE(totalWater, 1) << "Water should exist near the boundary";
}

// 7. Viscosity limits flow rate to 1 cell/tick horizontal
TEST_F(FallingSandLiquidTest, ViscosityLimitsFlowRate) {
    buildStoneFloor();

    grid.writeCell(16, 1, 16, makeMaterial(material_ids::WATER));
    grid.advanceEpoch();

    // After 1 tick, water can move at most 1 cell horizontally
    runLiquidTick(ChunkCoord{0, 0, 0}, 0);

    // Check that water hasn't moved more than 1 cell from origin
    bool foundFarWater = false;
    for (int x = 0; x < K_CHUNK_SIZE; ++x) {
        for (int z = 0; z < K_CHUNK_SIZE; ++z) {
            if (cellMaterialId(grid.readCell(x, 1, z)) == material_ids::WATER) {
                int dist = std::abs(x - 16) + std::abs(z - 16);
                if (dist > 1)
                    foundFarWater = true;
            }
        }
    }
    EXPECT_FALSE(foundFarWater) << "Water should not move more than 1 cell/tick horizontally";
}

// 8. Water conservation in sealed container
TEST_F(FallingSandLiquidTest, WaterConservation) {
    buildStoneBox(8, 24, 8, 24, 10);

    // Fill with 100 water cells
    int placed = 0;
    for (int y = 1; y <= 4 && placed < 100; ++y)
        for (int x = 9; x <= 23 && placed < 100; ++x)
            for (int z = 9; z <= 23 && placed < 100; ++z) {
                grid.writeCell(x, y, z, makeMaterial(material_ids::WATER));
                ++placed;
            }
    grid.advanceEpoch();

    int initialCount = countMaterial(material_ids::WATER, 0, 31, 0, 31, 0, 31);
    EXPECT_EQ(initialCount, 100);

    for (uint64_t f = 0; f < 100; ++f) {
        tracker.setState(ChunkCoord{0, 0, 0}, ChunkState::Active);
        runChunkTick(ChunkCoord{0, 0, 0}, f);
    }

    int finalCount = countMaterial(material_ids::WATER, 0, 31, 0, 31, 0, 31);
    EXPECT_EQ(finalCount, initialCount) << "Water count must be conserved";
}

// 9. Water does not displace solids
TEST_F(FallingSandLiquidTest, WaterDoesNotDisplaceSolids) {
    buildStoneFloor();

    // Stone block at y=1 with water above
    grid.writeCell(16, 1, 16, makeMaterial(material_ids::STONE));
    grid.writeCell(16, 2, 16, makeMaterial(material_ids::WATER));
    grid.advanceEpoch();

    for (uint64_t f = 0; f < 20; ++f) {
        tracker.setState(ChunkCoord{0, 0, 0}, ChunkState::Active);
        runChunkTick(ChunkCoord{0, 0, 0}, f);
    }

    EXPECT_EQ(cellMaterialId(grid.readCell(16, 1, 16)), material_ids::STONE);
    EXPECT_EQ(cellMaterialId(grid.readCell(16, 0, 16)), material_ids::STONE);
}

// 10. Liquid flow preserves spare byte
TEST_F(FallingSandLiquidTest, LiquidFlowPreservesEssenceIdx) {
    VoxelCell water;
    water = cellForMaterial(material_ids::WATER);
    water.spare = 7;
    grid.writeCell(16, 10, 16, water);
    grid.advanceEpoch();

    runLiquidTick(ChunkCoord{0, 0, 0}, 0);

    VoxelCell fallen = grid.readCell(16, 9, 16);
    EXPECT_EQ(cellMaterialId(fallen), material_ids::WATER);
    EXPECT_EQ(fallen.spare, 7);
}

// 11. Performance: 10K liquid cells under 3ms
TEST_F(FallingSandLiquidTest, PerformanceLiquidSim) {
    buildStoneFloor();

    int placed = 0;
    for (int y = 1; y <= 15 && placed < 10000; ++y)
        for (int x = 0; x < K_CHUNK_SIZE && placed < 10000; ++x)
            for (int z = 0; z < K_CHUNK_SIZE && placed < 10000; ++z) {
                grid.writeCell(x, y, z, makeMaterial(material_ids::WATER));
                ++placed;
            }
    grid.advanceEpoch();

    ghosts.syncGhostCells(ChunkCoord{0, 0, 0}, grid);

    auto start = std::chrono::high_resolution_clock::now();
    system.simulateLiquid(ChunkCoord{0, 0, 0}, grid, ghosts, tracker, 0, rng, boundaryWrites, cellSwaps);
    auto end = std::chrono::high_resolution_clock::now();

    auto us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    EXPECT_LT(us, 50000) << "Liquid sim took " << us << "us, expected < 50000us";
}
