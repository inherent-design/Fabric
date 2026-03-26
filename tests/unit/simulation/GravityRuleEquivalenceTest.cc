/// Equivalence validation tests for Wave 5a: gravity rules absorbed into
/// WorldRuleEngine and evaluated by TransformationPass::gravityEvaluation.
///
/// These tests verify the key invariants that FallingSandSystem previously
/// guaranteed, ensuring the new gravity rule evaluation produces equivalent
/// physical behavior: matter conservation, displacement ordering, no double-move,
/// and correct boundary write semantics.
///
/// CRITICAL: If any of these tests fail, do NOT delete FallingSandSystem files.

#include "recurse/simulation/CellAccessors.hh"
#include "recurse/simulation/ChunkActivityTracker.hh"
#include "recurse/simulation/GhostCells.hh"
#include "recurse/simulation/MaterialRegistry.hh"
#include "recurse/simulation/SimulationGrid.hh"
#include "recurse/simulation/TransformationPass.hh"
#include "recurse/simulation/VoxelSimulationSystem.hh"
#include "recurse/simulation/WorldRuleEngine.hh"
#include <gtest/gtest.h>
#include <random>

using namespace recurse::simulation;

class GravityRuleEquivalenceTest : public ::testing::Test {
  protected:
    MaterialRegistry registry;
    SimulationGrid grid;
    ChunkActivityTracker tracker;
    GhostCellManager ghosts;
    WorldRuleEngine ruleEngine;
    TransformationPass transformPass{ruleEngine, registry, grid, ghosts, tracker};
    BoundaryWriteQueue boundaryWrites;
    std::vector<CellSwap> cellSwaps;
    std::mt19937 rng{42};

    void SetUp() override {
        grid.fillChunk(0, 0, 0, VoxelCell{});
        grid.materializeChunk(0, 0, 0);
        tracker.setState(ChunkCoord{0, 0, 0}, ChunkState::Active);
        activateAllSubRegions(ChunkCoord{0, 0, 0});
    }

    void activateAllSubRegions(ChunkCoord pos) {
        for (int lz = 0; lz < K_CHUNK_SIZE; lz += 8)
            for (int ly = 0; ly < K_CHUNK_SIZE; ly += 8)
                for (int lx = 0; lx < K_CHUNK_SIZE; lx += 8)
                    tracker.markSubRegionActive(pos, lx, ly, lz);
    }

    VoxelCell makeMaterial(MaterialId id) { return cellForMaterial(id); }

    void runGravityTick(ChunkCoord pos) {
        ghosts.syncGhostCells(pos, grid);
        boundaryWrites.clear();
        cellSwaps.clear();
        transformPass.gravityEvaluation(pos, rng, boundaryWrites, cellSwaps);
        grid.advanceEpoch();
    }

    void buildStoneFloor() {
        for (int x = 0; x < K_CHUNK_SIZE; ++x)
            for (int z = 0; z < K_CHUNK_SIZE; ++z)
                grid.writeCell(x, 0, z, makeMaterial(material_ids::STONE));
        grid.advanceEpoch();
    }

    void buildStoneBox(int xmin, int xmax, int zmin, int zmax, int h) {
        for (int x = xmin; x <= xmax; ++x) {
            for (int z = zmin; z <= zmax; ++z) {
                grid.writeCell(x, 0, z, makeMaterial(material_ids::STONE));
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

    int countAllMaterial(MaterialId id) {
        return countMaterial(id, 0, K_CHUNK_SIZE - 1, 0, K_CHUNK_SIZE - 1, 0, K_CHUNK_SIZE - 1);
    }
};

// ---- Powder gravity equivalence ----

TEST_F(GravityRuleEquivalenceTest, PowderFallsOneCellPerTick) {
    grid.writeCell(16, 31, 16, makeMaterial(material_ids::SAND));
    grid.advanceEpoch();

    runGravityTick(ChunkCoord{0, 0, 0});

    EXPECT_EQ(cellMaterialId(grid.readCell(16, 30, 16)), material_ids::SAND) << "Powder should fall exactly 1 cell";
    EXPECT_EQ(cellMaterialId(grid.readCell(16, 31, 16)), material_ids::AIR) << "Source should be empty after fall";
}

TEST_F(GravityRuleEquivalenceTest, PowderSettlesOnFloor) {
    buildStoneFloor();

    grid.writeCell(16, 5, 16, makeMaterial(material_ids::SAND));
    grid.advanceEpoch();

    for (int i = 0; i < 10; ++i)
        runGravityTick(ChunkCoord{0, 0, 0});

    EXPECT_EQ(cellMaterialId(grid.readCell(16, 1, 16)), material_ids::SAND) << "Sand should rest on floor at y=1";
    EXPECT_EQ(cellMaterialId(grid.readCell(16, 0, 16)), material_ids::STONE) << "Floor should remain stone";
}

TEST_F(GravityRuleEquivalenceTest, PowderDoesNotDisplaceSolid) {
    buildStoneFloor();
    grid.writeCell(16, 1, 16, makeMaterial(material_ids::STONE));
    grid.writeCell(16, 2, 16, makeMaterial(material_ids::SAND));
    grid.advanceEpoch();

    for (int i = 0; i < 20; ++i) {
        tracker.setState(ChunkCoord{0, 0, 0}, ChunkState::Active);
        runGravityTick(ChunkCoord{0, 0, 0});
    }

    EXPECT_EQ(cellMaterialId(grid.readCell(16, 1, 16)), material_ids::STONE) << "Sand cannot displace stone below it";
}

TEST_F(GravityRuleEquivalenceTest, PowderMatterConservationInSealedBox) {
    buildStoneBox(8, 24, 8, 24, 15);

    int placed = 0;
    for (int y = 1; y <= 10 && placed < 200; ++y)
        for (int x = 9; x <= 23 && placed < 200; ++x)
            for (int z = 9; z <= 23 && placed < 200; ++z) {
                grid.writeCell(x, y, z, makeMaterial(material_ids::SAND));
                ++placed;
            }
    grid.advanceEpoch();

    int initialCount = countAllMaterial(material_ids::SAND);
    ASSERT_EQ(initialCount, 200);

    for (int i = 0; i < 100; ++i) {
        tracker.setState(ChunkCoord{0, 0, 0}, ChunkState::Active);
        runGravityTick(ChunkCoord{0, 0, 0});
    }

    int finalCount = countAllMaterial(material_ids::SAND);
    EXPECT_EQ(finalCount, initialCount) << "Powder count must be conserved in sealed box";
}

TEST_F(GravityRuleEquivalenceTest, DensityOrderingGravelBelowSand) {
    buildStoneFloor();
    // Contain column to prevent diagonal escape
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

    for (int i = 0; i < 10; ++i)
        runGravityTick(ChunkCoord{0, 0, 0});

    EXPECT_EQ(cellMaterialId(grid.readCell(16, 1, 16)), material_ids::GRAVEL) << "Denser gravel should sink below sand";
    EXPECT_EQ(cellMaterialId(grid.readCell(16, 2, 16)), material_ids::SAND)
        << "Less-dense sand should float above gravel";
}

// ---- Liquid gravity equivalence ----

TEST_F(GravityRuleEquivalenceTest, LiquidFallsInAir) {
    grid.writeCell(16, 10, 16, makeMaterial(material_ids::WATER));
    grid.advanceEpoch();

    runGravityTick(ChunkCoord{0, 0, 0});

    EXPECT_EQ(cellMaterialId(grid.readCell(16, 9, 16)), material_ids::WATER) << "Water should fall 1 cell";
    EXPECT_EQ(cellMaterialId(grid.readCell(16, 10, 16)), material_ids::AIR);
}

TEST_F(GravityRuleEquivalenceTest, LiquidSpreadsOnFlatSurface) {
    buildStoneFloor();
    grid.writeCell(16, 1, 16, makeMaterial(material_ids::WATER));
    grid.advanceEpoch();

    for (int i = 0; i < 10; ++i)
        runGravityTick(ChunkCoord{0, 0, 0});

    // Water should have moved from origin
    EXPECT_NE(cellMaterialId(grid.readCell(16, 1, 16)), material_ids::WATER)
        << "Water should have moved from origin after 10 ticks";

    // Water should still exist at y=1
    bool anyWaterAtY1 = false;
    for (int x = 0; x < K_CHUNK_SIZE && !anyWaterAtY1; ++x)
        for (int z = 0; z < K_CHUNK_SIZE && !anyWaterAtY1; ++z)
            if (cellMaterialId(grid.readCell(x, 1, z)) == material_ids::WATER)
                anyWaterAtY1 = true;
    EXPECT_TRUE(anyWaterAtY1) << "Water should exist at y=1 level";
}

TEST_F(GravityRuleEquivalenceTest, LiquidMatterConservationInSealedBox) {
    buildStoneBox(10, 14, 10, 14, 4);

    // Pour 9 cells of water
    for (int x = 11; x <= 13; ++x)
        for (int z = 11; z <= 13; ++z)
            grid.writeCell(x, 4, z, makeMaterial(material_ids::WATER));
    grid.advanceEpoch();

    int initialCount = countAllMaterial(material_ids::WATER);
    ASSERT_EQ(initialCount, 9);

    for (int i = 0; i < 50; ++i) {
        tracker.setState(ChunkCoord{0, 0, 0}, ChunkState::Active);
        runGravityTick(ChunkCoord{0, 0, 0});
    }

    int finalCount = countAllMaterial(material_ids::WATER);
    EXPECT_EQ(finalCount, initialCount) << "Liquid count must be conserved in sealed box";

    // Water should have settled at bottom
    int bottomWater = countMaterial(material_ids::WATER, 11, 13, 1, 1, 11, 13);
    EXPECT_EQ(bottomWater, 9) << "All water should settle at bottom of container";
}

TEST_F(GravityRuleEquivalenceTest, LiquidDoesNotDisplaceSolid) {
    buildStoneFloor();
    grid.writeCell(16, 1, 16, makeMaterial(material_ids::STONE));
    grid.writeCell(16, 2, 16, makeMaterial(material_ids::WATER));
    grid.advanceEpoch();

    for (int i = 0; i < 20; ++i) {
        tracker.setState(ChunkCoord{0, 0, 0}, ChunkState::Active);
        runGravityTick(ChunkCoord{0, 0, 0});
    }

    EXPECT_EQ(cellMaterialId(grid.readCell(16, 1, 16)), material_ids::STONE) << "Water cannot displace stone";
}

TEST_F(GravityRuleEquivalenceTest, LiquidBlockedByWalls) {
    buildStoneBox(10, 14, 10, 14, 4);
    grid.writeCell(12, 1, 12, makeMaterial(material_ids::WATER));
    grid.advanceEpoch();

    for (int i = 0; i < 50; ++i)
        runGravityTick(ChunkCoord{0, 0, 0});

    int insideWater = countMaterial(material_ids::WATER, 11, 13, 1, 3, 11, 13);
    EXPECT_EQ(insideWater, 1) << "Single water cell should remain inside sealed box";

    int outsideWater =
        countMaterial(material_ids::WATER, 0, 9, 0, K_CHUNK_SIZE - 1, 0, K_CHUNK_SIZE - 1) +
        countMaterial(material_ids::WATER, 15, K_CHUNK_SIZE - 1, 0, K_CHUNK_SIZE - 1, 0, K_CHUNK_SIZE - 1);
    EXPECT_EQ(outsideWater, 0) << "No water should escape sealed box";
}

// ---- U-tube equivalence ----

TEST_F(GravityRuleEquivalenceTest, LiquidFindsLevelInUTube) {
    buildStoneFloor();

    // Left chamber: x=[5..9], right chamber: x=[11..15], divider at x=10 y=2..5 (gap at y=1)
    for (int z = 14; z <= 18; ++z) {
        for (int y = 1; y <= 5; ++y) {
            grid.writeCell(5, y, z, makeMaterial(material_ids::STONE));
            grid.writeCell(15, y, z, makeMaterial(material_ids::STONE));
            for (int x = 5; x <= 15; ++x) {
                grid.writeCell(x, y, 14, makeMaterial(material_ids::STONE));
                grid.writeCell(x, y, 18, makeMaterial(material_ids::STONE));
            }
        }
        for (int y = 2; y <= 5; ++y)
            grid.writeCell(10, y, z, makeMaterial(material_ids::STONE));
    }
    grid.advanceEpoch();

    // 10 water cells in left chamber
    for (int y = 1; y <= 2; ++y)
        for (int x = 6; x <= 9; ++x)
            grid.writeCell(x, y, 16, makeMaterial(material_ids::WATER));
    grid.writeCell(6, 3, 16, makeMaterial(material_ids::WATER));
    grid.writeCell(7, 3, 16, makeMaterial(material_ids::WATER));
    grid.advanceEpoch();

    int totalBefore = countAllMaterial(material_ids::WATER);
    ASSERT_GT(totalBefore, 0);

    for (int i = 0; i < 200; ++i) {
        tracker.setState(ChunkCoord{0, 0, 0}, ChunkState::Active);
        runGravityTick(ChunkCoord{0, 0, 0});
    }

    int rightWater = countMaterial(material_ids::WATER, 11, 14, 1, 5, 15, 17);
    EXPECT_GT(rightWater, 0) << "Water should flow through gap to right chamber";

    int totalAfter = countAllMaterial(material_ids::WATER);
    EXPECT_EQ(totalAfter, totalBefore) << "Water must be conserved through U-tube flow";
}

// ---- Cross-chunk boundary equivalence ----

TEST_F(GravityRuleEquivalenceTest, CrossChunkFallingProducesBoundaryWrite) {
    grid.fillChunk(0, 1, 0, VoxelCell{});
    grid.materializeChunk(0, 1, 0);
    tracker.setState(ChunkCoord{0, 1, 0}, ChunkState::Active);
    activateAllSubRegions(ChunkCoord{0, 1, 0});

    // Sand at chunk(0,1,0) local y=0 = world y=32
    grid.writeCell(16, 32, 16, makeMaterial(material_ids::SAND));
    grid.advanceEpoch();

    // Simulate chunk(0,1,0) -- sand at y=32 should fall to y=31 (chunk 0,0,0)
    ghosts.syncGhostCells(ChunkCoord{0, 1, 0}, grid);
    boundaryWrites.clear();
    cellSwaps.clear();
    transformPass.gravityEvaluation(ChunkCoord{0, 1, 0}, rng, boundaryWrites, cellSwaps);

    // Should produce exactly 1 boundary write (cross-chunk destination)
    EXPECT_EQ(boundaryWrites.size(), 1u) << "Cross-chunk fall should produce exactly 1 boundary write";

    // Drain boundary writes
    for (const auto& bw : boundaryWrites) {
        if (!grid.writeCellIfExists(bw.dstWx, bw.dstWy, bw.dstWz, bw.writeCell))
            grid.writeCell(bw.srcWx, bw.srcWy, bw.srcWz, bw.undoCell);
    }
    boundaryWrites.clear();
    grid.advanceEpoch();

    EXPECT_EQ(cellMaterialId(grid.readCell(16, 31, 16)), material_ids::SAND)
        << "Sand should fall from chunk (0,1,0) y=32 to chunk (0,0,0) y=31";
    EXPECT_EQ(cellMaterialId(grid.readCell(16, 32, 16)), material_ids::AIR) << "Source should be empty";
}

// ---- No double-move equivalence ----

TEST_F(GravityRuleEquivalenceTest, NoDoubleMoveInSingleTick) {
    buildStoneFloor();

    // Place a column of 3 sand grains at (16, 2), (16, 3), (16, 4)
    grid.writeCell(16, 2, 16, makeMaterial(material_ids::SAND));
    grid.writeCell(16, 3, 16, makeMaterial(material_ids::SAND));
    grid.writeCell(16, 4, 16, makeMaterial(material_ids::SAND));
    grid.advanceEpoch();

    int beforeCount = countAllMaterial(material_ids::SAND);
    ASSERT_EQ(beforeCount, 3);

    runGravityTick(ChunkCoord{0, 0, 0});

    int afterCount = countAllMaterial(material_ids::SAND);
    EXPECT_EQ(afterCount, beforeCount) << "Sand count must be conserved";

    // At least one grain should have moved (bottom grain at y=2 can fall to y=1)
    bool anyMoved = (cellMaterialId(grid.readCell(16, 1, 16)) == material_ids::SAND) ||
                    (cellMaterialId(grid.readCell(16, 2, 16)) != material_ids::SAND) ||
                    (cellMaterialId(grid.readCell(16, 3, 16)) != material_ids::SAND);
    EXPECT_TRUE(anyMoved) << "At least one sand grain should move in single tick";
}

// ---- Gas rise equivalence ----

TEST_F(GravityRuleEquivalenceTest, GasRisesInAir) {
    // Stone floor and ceiling to contain gas
    for (int x = 0; x < K_CHUNK_SIZE; ++x)
        for (int z = 0; z < K_CHUNK_SIZE; ++z) {
            grid.writeCell(x, 0, z, makeMaterial(material_ids::STONE));
            grid.writeCell(x, 5, z, makeMaterial(material_ids::STONE));
        }
    grid.advanceEpoch();

    // Manually create a gas cell (no Gas material exists yet, set phase directly)
    VoxelCell gas = makeMaterial(material_ids::AIR);
    gas.essenceIdx = static_cast<uint8_t>(material_ids::STONE); // Non-zero so not empty
    gas.displacementRank = 50;
    gas.setPhase(Phase::Gas);
    grid.writeCell(16, 1, 16, gas);
    grid.advanceEpoch();

    for (int i = 0; i < 10; ++i)
        runGravityTick(ChunkCoord{0, 0, 0});

    // Gas should have risen toward the ceiling (y=1 -> y=2 or higher)
    bool gasAboveOrigin = false;
    for (int y = 2; y <= 4; ++y) {
        VoxelCell c = grid.readCell(16, y, 16);
        if (c.phase() == Phase::Gas) {
            gasAboveOrigin = true;
            break;
        }
    }
    EXPECT_TRUE(gasAboveOrigin) << "Gas should rise above its starting position";
}

// ---- Settled chunk equivalence ----

TEST_F(GravityRuleEquivalenceTest, AllStoneChunkReturnsNoMovement) {
    for (int z = 0; z < K_CHUNK_SIZE; ++z)
        for (int y = 0; y < K_CHUNK_SIZE; ++y)
            for (int x = 0; x < K_CHUNK_SIZE; ++x)
                grid.writeCell(x, y, z, makeMaterial(material_ids::STONE));
    grid.advanceEpoch();

    tracker.setState(ChunkCoord{0, 0, 0}, ChunkState::Active);
    ghosts.syncGhostCells(ChunkCoord{0, 0, 0}, grid);
    boundaryWrites.clear();
    cellSwaps.clear();
    bool changed = transformPass.gravityEvaluation(ChunkCoord{0, 0, 0}, rng, boundaryWrites, cellSwaps);
    grid.advanceEpoch();

    EXPECT_FALSE(changed) << "All-solid chunk should report no movement";
    EXPECT_TRUE(cellSwaps.empty()) << "All-solid chunk should produce zero cell swaps";
    EXPECT_TRUE(boundaryWrites.empty()) << "All-solid chunk should produce zero boundary writes";
}

// ---- CellSwap tracking equivalence ----

TEST_F(GravityRuleEquivalenceTest, SinglePowderFallRecordsTwoCellSwaps) {
    grid.writeCell(16, 31, 16, makeMaterial(material_ids::SAND));
    grid.advanceEpoch();

    ghosts.syncGhostCells(ChunkCoord{0, 0, 0}, grid);
    boundaryWrites.clear();
    cellSwaps.clear();
    transformPass.gravityEvaluation(ChunkCoord{0, 0, 0}, rng, boundaryWrites, cellSwaps);

    // One swap = 2 CellSwap entries (source cleared, dest filled)
    EXPECT_EQ(cellSwaps.size(), 2u) << "Single fall should produce exactly 2 CellSwap entries";
}

// ---- Spare byte preservation equivalence ----

TEST_F(GravityRuleEquivalenceTest, SpareBytePreservedOnFall) {
    VoxelCell water = makeMaterial(material_ids::WATER);
    water.spare = 99;
    grid.writeCell(16, 10, 16, water);
    grid.advanceEpoch();

    runGravityTick(ChunkCoord{0, 0, 0});

    VoxelCell fallen = grid.readCell(16, 9, 16);
    EXPECT_EQ(cellMaterialId(fallen), material_ids::WATER);
    EXPECT_EQ(fallen.spare, 99) << "Spare byte must be preserved through gravity swap";
}

TEST_F(GravityRuleEquivalenceTest, SpareBytePreservedOnDensitySwap) {
    buildStoneFloor();
    // Contain column
    for (int y = 1; y <= 4; ++y) {
        grid.writeCell(15, y, 16, makeMaterial(material_ids::STONE));
        grid.writeCell(17, y, 16, makeMaterial(material_ids::STONE));
        grid.writeCell(16, y, 15, makeMaterial(material_ids::STONE));
        grid.writeCell(16, y, 17, makeMaterial(material_ids::STONE));
    }
    grid.advanceEpoch();

    VoxelCell sand = makeMaterial(material_ids::SAND);
    sand.spare = 10;
    grid.writeCell(16, 1, 16, sand);

    VoxelCell gravel = makeMaterial(material_ids::GRAVEL);
    gravel.spare = 20;
    grid.writeCell(16, 2, 16, gravel);
    grid.advanceEpoch();

    for (int i = 0; i < 10; ++i)
        runGravityTick(ChunkCoord{0, 0, 0});

    VoxelCell bottom = grid.readCell(16, 1, 16);
    VoxelCell top = grid.readCell(16, 2, 16);
    EXPECT_EQ(cellMaterialId(bottom), material_ids::GRAVEL);
    EXPECT_EQ(bottom.spare, 20) << "Gravel spare byte must be preserved";
    EXPECT_EQ(cellMaterialId(top), material_ids::SAND);
    EXPECT_EQ(top.spare, 10) << "Sand spare byte must be preserved";
}
