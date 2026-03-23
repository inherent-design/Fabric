#include "recurse/simulation/TransformationPass.hh"
#include "recurse/simulation/CellAccessors.hh"
#include "recurse/simulation/ProjectionRuleTable.hh"
#include "recurse/simulation/VoxelSimulationSystem.hh"
#include <gtest/gtest.h>

using namespace recurse::simulation;

class TransformationPassTest : public ::testing::Test {
  protected:
    VoxelSimulationSystem sim;

    void SetUp() override {
        sim.scheduler().disableForTesting();
        sim.grid().fillChunk(0, 0, 0, VoxelCell{});
        sim.grid().materializeChunk(0, 0, 0);
        sim.activityTracker().setState(ChunkCoord{0, 0, 0}, ChunkState::Active);
        markAllSubRegions(ChunkCoord{0, 0, 0});
    }

    void markAllSubRegions(ChunkCoord pos) {
        for (int lz = 0; lz < K_CHUNK_SIZE; lz += 8)
            for (int ly = 0; ly < K_CHUNK_SIZE; ly += 8)
                for (int lx = 0; lx < K_CHUNK_SIZE; lx += 8)
                    sim.activityTracker().markSubRegionActive(pos, lx, ly, lz);
    }

    void syncGhostsForOrigin() {
        std::vector<ChunkCoord> positions = {ChunkCoord{0, 0, 0}};
        sim.ghostCellManager().syncAll(positions, sim.grid());
    }
};

// 1. Thermal diffusion moves heat from a hot cell toward cooler neighbors.
TEST_F(TransformationPassTest, ThermalDiffusionBasic) {
    // Fill a 5x5x5 block of ambient stone so neighbors have uniform context.
    VoxelCell ambient = makeCell(1, Phase::Solid, 200);
    setCellTemperature(ambient, 100);
    for (int x = 14; x <= 18; ++x)
        for (int y = 14; y <= 18; ++y)
            for (int z = 14; z <= 18; ++z)
                sim.grid().writeCell(x, y, z, ambient);

    // Place a hot cell at the center
    VoxelCell hot = makeCell(1, Phase::Solid, 200);
    setCellTemperature(hot, 200);
    sim.grid().writeCell(16, 16, 16, hot);
    sim.grid().advanceEpoch();

    syncGhostsForOrigin();

    TransformationPass pass(sim.ruleEngine(), sim.materials(), sim.grid(), sim.ghostCellManager(),
                            sim.activityTracker());
    std::mt19937 rng(42);
    pass.executeChunk(ChunkCoord{0, 0, 0}, rng);

    // Hot cell should have cooled
    VoxelCell result = sim.grid().readFromWriteBuffer(16, 16, 16);
    EXPECT_LT(cellTemperature(result), 200) << "Hot cell should cool toward neighbors";

    // At least one direct neighbor should have warmed above 100
    static constexpr int offsets[6][3] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
    bool anyWarmed = false;
    for (const auto& off : offsets) {
        VoxelCell n = sim.grid().readFromWriteBuffer(16 + off[0], 16 + off[1], 16 + off[2]);
        if (cellTemperature(n) > 100) {
            anyWarmed = true;
            break;
        }
    }
    EXPECT_TRUE(anyWarmed) << "At least one neighbor should warm due to diffusion";
}

// 2. Thermally uniform chunk with uniform ghost cells is fully skipped.
TEST_F(TransformationPassTest, ThermalSkipStable) {
    // Fill chunk and all 6 face-neighbor chunks with uniform temperature stone
    // so ghost cells also read temp=100 and no cell has a temperature gradient.
    VoxelCell uniform = makeCell(1, Phase::Solid, 200);
    setCellTemperature(uniform, 100);

    // Fill origin chunk
    for (int x = 0; x < K_CHUNK_SIZE; ++x)
        for (int y = 0; y < K_CHUNK_SIZE; ++y)
            for (int z = 0; z < K_CHUNK_SIZE; ++z)
                sim.grid().writeCell(x, y, z, uniform);

    // Fill 6 face-neighbor chunks (just the boundary slice facing origin)
    static constexpr int neighborOffsets[6][3] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
    for (const auto& off : neighborOffsets) {
        int ncx = off[0], ncy = off[1], ncz = off[2];
        sim.grid().fillChunk(ncx, ncy, ncz, uniform);
        sim.grid().materializeChunk(ncx, ncy, ncz);
        // Write to write buffer so syncChunkBuffers copies it
        for (int x = 0; x < K_CHUNK_SIZE; ++x)
            for (int y = 0; y < K_CHUNK_SIZE; ++y)
                for (int z = 0; z < K_CHUNK_SIZE; ++z)
                    sim.grid().writeCell(ncx * K_CHUNK_SIZE + x, ncy * K_CHUNK_SIZE + y, ncz * K_CHUNK_SIZE + z,
                                         uniform);
    }
    sim.grid().advanceEpoch();

    // Sync ghost cells so boundary reads see uniform temp
    std::vector<ChunkCoord> positions = {ChunkCoord{0, 0, 0}};
    sim.ghostCellManager().syncAll(positions, sim.grid());

    TransformationPass pass(sim.ruleEngine(), sim.materials(), sim.grid(), sim.ghostCellManager(),
                            sim.activityTracker());
    std::mt19937 rng(42);
    pass.executeChunk(ChunkCoord{0, 0, 0}, rng);

    // Interior AND boundary cells should all be unchanged
    VoxelCell inner = sim.grid().readFromWriteBuffer(16, 16, 16);
    EXPECT_EQ(cellTemperature(inner), 100) << "Interior cell in fully uniform chunk should not change";

    VoxelCell boundary = sim.grid().readFromWriteBuffer(0, 0, 0);
    EXPECT_EQ(cellTemperature(boundary), 100) << "Boundary cell with uniform ghost cells should not change";
}

// 3. Water below freeze threshold triggers ice rule.
TEST_F(TransformationPassTest, RuleFiringBasic) {
    // Place water cell at temp=80 (below freeze threshold of 90)
    VoxelCell water = makeCell(4, Phase::Liquid, 100);
    setCellTemperature(water, 80);
    sim.grid().writeCell(16, 16, 16, water);
    sim.grid().advanceEpoch();

    syncGhostsForOrigin();

    // Run rule evaluation multiple times (50% probability per attempt)
    bool frozen = false;
    for (int attempt = 0; attempt < 50 && !frozen; ++attempt) {
        // Reset cell to water each attempt
        sim.grid().writeCell(16, 16, 16, water);

        TransformationPass pass(sim.ruleEngine(), sim.materials(), sim.grid(), sim.ghostCellManager(),
                                sim.activityTracker());
        std::mt19937 rng(static_cast<uint32_t>(attempt * 7 + 13));
        pass.executeChunk(ChunkCoord{0, 0, 0}, rng);

        VoxelCell result = sim.grid().readFromWriteBuffer(16, 16, 16);
        if (result.essenceIdx == 6 && result.phase() == Phase::Solid) {
            frozen = true;
        }
    }
    EXPECT_TRUE(frozen) << "Water at temp=80 should eventually freeze (50% probability per tick)";
}

// 4. Budget cap limits transformations per chunk.
TEST_F(TransformationPassTest, BudgetCap) {
    // Fill chunk with water below freeze threshold
    VoxelCell water = makeCell(4, Phase::Liquid, 100);
    setCellTemperature(water, 80);

    int placed = 0;
    for (int y = 0; y < K_CHUNK_SIZE && placed < 200; ++y)
        for (int z = 0; z < K_CHUNK_SIZE && placed < 200; ++z)
            for (int x = 0; x < K_CHUNK_SIZE && placed < 200; ++x) {
                sim.grid().writeCell(x, y, z, water);
                ++placed;
            }
    sim.grid().advanceEpoch();

    syncGhostsForOrigin();

    TransformationPass pass(sim.ruleEngine(), sim.materials(), sim.grid(), sim.ghostCellManager(),
                            sim.activityTracker());
    std::mt19937 rng(1);
    pass.executeChunk(ChunkCoord{0, 0, 0}, rng);

    EXPECT_LE(pass.totalTransforms(), K_MAX_TRANSFORMS_PER_CHUNK)
        << "Transformations per chunk must not exceed budget cap";
}

// 5. Integration test: tick() runs Phase 3c and produces transformations.
TEST_F(TransformationPassTest, ExecuteIntegration) {
    VoxelCell stone = makeCell(1, Phase::Solid, 200);
    setCellTemperature(stone, 50);
    VoxelCell water = makeCell(4, Phase::Liquid, 100);
    setCellTemperature(water, 50);

    // Build a stone containment box so FallingSand doesn't move water away
    // Floor
    for (int x = 14; x <= 18; ++x)
        for (int z = 14; z <= 18; ++z)
            sim.grid().writeCell(x, 0, z, stone);
    // Walls (y=1..3)
    for (int y = 1; y <= 3; ++y) {
        for (int x = 14; x <= 18; ++x) {
            sim.grid().writeCell(x, y, 14, stone);
            sim.grid().writeCell(x, y, 18, stone);
        }
        for (int z = 15; z <= 17; ++z) {
            sim.grid().writeCell(14, y, z, stone);
            sim.grid().writeCell(18, y, z, stone);
        }
    }
    // Place water inside the box
    for (int x = 15; x <= 17; ++x)
        for (int z = 15; z <= 17; ++z)
            sim.grid().writeCell(x, 1, z, water);
    sim.grid().advanceEpoch();

    bool frozen = false;
    for (int i = 0; i < 100 && !frozen; ++i) {
        sim.activityTracker().setState(ChunkCoord{0, 0, 0}, ChunkState::Active);
        markAllSubRegions(ChunkCoord{0, 0, 0});
        sim.tick();

        // Check all water positions inside the box
        for (int x = 15; x <= 17 && !frozen; ++x)
            for (int z = 15; z <= 17 && !frozen; ++z) {
                VoxelCell cell = sim.grid().readCell(x, 1, z);
                if (cell.essenceIdx == 6 && cell.phase() == Phase::Solid)
                    frozen = true;
            }
    }
    EXPECT_TRUE(frozen) << "Contained water at temp=50 should freeze via Phase 3c";
}

// 6. A cell surrounded by empty air retains its temperature.
TEST_F(TransformationPassTest, ThermalIsolationInAir) {
    VoxelCell hot = makeCell(1, Phase::Solid, 200);
    setCellTemperature(hot, 200);
    sim.grid().writeCell(16, 16, 16, hot);
    sim.grid().advanceEpoch();

    syncGhostsForOrigin();

    TransformationPass pass(sim.ruleEngine(), sim.materials(), sim.grid(), sim.ghostCellManager(),
                            sim.activityTracker());
    std::mt19937 rng(42);
    pass.executeChunk(ChunkCoord{0, 0, 0}, rng);

    VoxelCell result = sim.grid().readFromWriteBuffer(16, 16, 16);
    EXPECT_EQ(cellTemperature(result), 200) << "Cell surrounded by empty air should retain temperature";
}

// 7. ProjectionRuleTable contains projected appearances for ice, glass, magma.
TEST_F(TransformationPassTest, ProjectionTablePopulated) {
    const auto& table = sim.projectionTable();

    const auto& ice = table.lookup(6, Phase::Solid);
    EXPECT_EQ(ice.baseColor, 0xFFD0E8FFu) << "ICE should have blue-white color";
    EXPECT_EQ(ice.moveType, MoveType::Static);

    const auto& glass = table.lookup(10, Phase::Solid);
    EXPECT_EQ(glass.baseColor, 0xFFA08040u) << "GLASS should have amber color";
    EXPECT_EQ(glass.moveType, MoveType::Static);

    const auto& magma = table.lookup(11, Phase::Liquid);
    EXPECT_EQ(magma.baseColor, 0xFFFF4400u) << "MAGMA should have orange-red color";
    EXPECT_EQ(magma.moveType, MoveType::Liquid);
}
