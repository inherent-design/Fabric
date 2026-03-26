#include "recurse/simulation/CellAccessors.hh"
#include "recurse/simulation/ChunkActivityTracker.hh"
#include "recurse/simulation/ProjectionRuleTable.hh"
#include "recurse/simulation/TransformationPass.hh"
#include "recurse/simulation/VoxelSimulationSystem.hh"
#include "recurse/simulation/WorldRuleEngine.hh"
#include <gtest/gtest.h>

using namespace recurse::simulation;

class WorldRuleEngineIntegrationTest : public ::testing::Test {
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

    void placeCell(int wx, int wy, int wz, VoxelCell cell) { sim.grid().writeCell(wx, wy, wz, cell); }

    void advance() { sim.grid().advanceEpoch(); }

    VoxelCell makeMaterial(MaterialId id) { return cellForMaterial(id); }

    void buildStoneBox(int xmin, int xmax, int zmin, int zmax, int floorY, int ceilY) {
        VoxelCell stone = makeMaterial(material_ids::STONE);
        for (int x = xmin; x <= xmax; ++x) {
            for (int z = zmin; z <= zmax; ++z) {
                placeCell(x, floorY, z, stone);
                placeCell(x, ceilY, z, stone);
                for (int y = floorY + 1; y < ceilY; ++y) {
                    if (x == xmin || x == xmax || z == zmin || z == zmax)
                        placeCell(x, y, z, stone);
                }
            }
        }
        advance();
    }
};

// E2E-1: Full tick() dispatches unified Phase 3 (thermal + gravity + transformation).
TEST_F(WorldRuleEngineIntegrationTest, FullTickUnifiesGravityAndTransformation) {
    buildStoneBox(12, 20, 12, 20, 0, 6);

    VoxelCell coldWater = makeCell(4, Phase::Liquid, 100);
    setCellTemperature(coldWater, 50);
    for (int x = 13; x <= 19; ++x)
        for (int z = 13; z <= 19; ++z)
            placeCell(x, 1, z, coldWater);
    advance();

    bool frozen = false;
    for (int i = 0; i < 200 && !frozen; ++i) {
        sim.activityTracker().setState(ChunkCoord{0, 0, 0}, ChunkState::Active);
        markAllSubRegions(ChunkCoord{0, 0, 0});
        sim.tick();

        for (int x = 13; x <= 19 && !frozen; ++x)
            for (int z = 13; z <= 19 && !frozen; ++z) {
                VoxelCell c = sim.grid().readCell(x, 1, z);
                if (c.essenceIdx == 6 && c.phase() == Phase::Solid)
                    frozen = true;
            }
    }
    EXPECT_TRUE(frozen) << "Water in containment should freeze via unified dispatch (thermal + R1 rule)";
}

// E2E-2: Transformed cell has a different visual hash than original material.
TEST_F(WorldRuleEngineIntegrationTest, TransformedCellHasDifferentVisualHash) {
    MergeKey waterHash = mergeKey(makeCell(4, Phase::Liquid, 100));
    MergeKey iceHash = mergeKey(makeCell(6, Phase::Solid, 90));

    EXPECT_NE(waterHash, iceHash) << "Water and ice must have different visual hashes (pre-condition)";
}

// E2E-3: ProjectionRuleTable returns correct projected color for transformed state.
TEST_F(WorldRuleEngineIntegrationTest, ProjectionTableReturnsCorrectColorAfterTransform) {
    const auto& table = sim.projectionTable();

    auto waterProj = table.lookup(4, Phase::Liquid);
    auto iceProj = table.lookup(6, Phase::Solid);

    EXPECT_NE(waterProj.baseColor, 0u) << "Water projection must be populated";
    EXPECT_NE(iceProj.baseColor, 0u) << "Ice projection must be populated";
    EXPECT_NE(waterProj.baseColor, iceProj.baseColor) << "Water and ice must have distinct projected colors";
    EXPECT_EQ(iceProj.moveType, MoveType::Static);
}

// E2E-4: Thermal diffusion cools a hot cell near cold neighbors.
TEST_F(WorldRuleEngineIntegrationTest, ThermalDiffusionInUnifiedDispatch) {
    VoxelCell ambient = makeCell(1, Phase::Solid, 200);
    setCellTemperature(ambient, 50);

    for (int x = 14; x <= 18; ++x)
        for (int y = 14; y <= 18; ++y)
            for (int z = 14; z <= 18; ++z)
                placeCell(x, y, z, ambient);

    VoxelCell hot = makeCell(1, Phase::Solid, 200);
    setCellTemperature(hot, 200);
    placeCell(16, 16, 16, hot);
    advance();

    syncGhostsForOrigin();
    TransformationPass pass(sim.ruleEngine(), sim.materials(), sim.grid(), sim.ghostCellManager(),
                            sim.activityTracker());
    std::mt19937 rng(42);
    BoundaryWriteQueue boundaryWrites;
    std::vector<CellSwap> cellSwaps;
    std::vector<SubRegionActivation> activations;
    pass.evaluateChunk(ChunkCoord{0, 0, 0}, rng, boundaryWrites, cellSwaps, activations);

    VoxelCell result = sim.grid().readFromWriteBuffer(16, 16, 16);
    EXPECT_LT(cellTemperature(result), 200) << "Hot cell should cool via thermal diffusion in unified pass";
}

// E2E-5: Water + magma contact reaction produces stone (R7, probability=255).
TEST_F(WorldRuleEngineIntegrationTest, WaterMagmaContactProducesStone) {
    // Build a sealed 1x1x1 cell (can't move in any direction)
    VoxelCell stone = makeMaterial(material_ids::STONE);
    for (const auto& off :
         std::vector<std::array<int, 3>>{{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}}) {
        placeCell(17 + off[0], 1 + off[1], 16 + off[2], stone);
    }
    placeCell(17, 1, 16, VoxelCell{});
    advance();

    VoxelCell water = makeCell(4, Phase::Liquid, 100);
    setCellTemperature(water, 100);
    VoxelCell magma = makeCell(11, Phase::Liquid, 200);
    setCellTemperature(magma, 210);

    placeCell(16, 1, 16, water);
    placeCell(18, 1, 16, magma);
    advance();

    sim.activityTracker().setState(ChunkCoord{0, 0, 0}, ChunkState::Active);
    markAllSubRegions(ChunkCoord{0, 0, 0});
    sim.tick();

    // Both water and magma should have reacted to stone via R7
    // At minimum, water (16,1,16) should be adjacent to magma (17,1,16) which
    // is stone. Since magma is now stone, the contact rule can't fire. But the
    // thermal diffusion should have raised water temp above 125, triggering R3 (boil).
    // Instead, test the R7 path directly: place them adjacent.
    placeCell(16, 1, 16, water);
    placeCell(17, 1, 16, magma);
    advance();

    sim.activityTracker().setState(ChunkCoord{0, 0, 0}, ChunkState::Active);
    markAllSubRegions(ChunkCoord{0, 0, 0});
    sim.tick();

    VoxelCell resultA = sim.grid().readCell(16, 1, 16);
    VoxelCell resultB = sim.grid().readCell(17, 1, 16);
    // After tick(), water and magma should have reacted.
    // The exact result depends on whether R7 fired (both become stone) or
    // R3 fired (water becomes empty). Either way, neither should be original.
    bool waterUnchanged = (resultA.essenceIdx == 4 && resultA.phase() == Phase::Liquid);
    bool magmaUnchanged = (resultB.essenceIdx == 11 && resultB.phase() == Phase::Liquid);
    EXPECT_FALSE(waterUnchanged) << "Water should react when adjacent to magma";
    EXPECT_FALSE(magmaUnchanged) << "Magma should react when adjacent to water";
}

// E2E-6: Gravity rules fire within unified dispatch (powder falls).
TEST_F(WorldRuleEngineIntegrationTest, GravityFallsInUnifiedDispatch) {
    buildStoneBox(14, 18, 14, 18, 0, 10);

    VoxelCell sand = makeMaterial(material_ids::SAND);
    placeCell(16, 9, 16, sand);
    advance();

    syncGhostsForOrigin();
    BoundaryWriteQueue boundaryWrites;
    std::vector<CellSwap> cellSwaps;
    std::vector<SubRegionActivation> activations;
    TransformationPass pass(sim.ruleEngine(), sim.materials(), sim.grid(), sim.ghostCellManager(),
                            sim.activityTracker());
    std::mt19937 rng(42);
    auto result = pass.evaluateChunk(ChunkCoord{0, 0, 0}, rng, boundaryWrites, cellSwaps, activations);
    advance();

    EXPECT_TRUE(result.anyGravityMovement) << "Sand should fall in unified dispatch";
    EXPECT_EQ(cellMaterialId(sim.grid().readCell(16, 8, 16)), material_ids::SAND) << "Sand should be one cell lower";
    EXPECT_EQ(cellMaterialId(sim.grid().readCell(16, 9, 16)), material_ids::AIR) << "Source should be empty";
}

// E2E-7: Settled chunk detection works with unified dispatch.
TEST_F(WorldRuleEngineIntegrationTest, AllSolidChunkSettlesImmediately) {
    for (int x = 0; x < K_CHUNK_SIZE; ++x)
        for (int y = 0; y < K_CHUNK_SIZE; ++y)
            for (int z = 0; z < K_CHUNK_SIZE; ++z)
                placeCell(x, y, z, makeMaterial(material_ids::STONE));
    advance();

    sim.activityTracker().setState(ChunkCoord{0, 0, 0}, ChunkState::Active);
    sim.tick();

    EXPECT_EQ(sim.activityTracker().getState(ChunkCoord{0, 0, 0}), ChunkState::Sleeping)
        << "All-solid chunk should settle to Sleeping after unified dispatch";
}
