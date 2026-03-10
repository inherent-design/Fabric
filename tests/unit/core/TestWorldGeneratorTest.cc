#include "recurse/world/TestWorldGenerator.hh"
#include "recurse/simulation/SimulationGrid.hh"
#include "recurse/simulation/VoxelMaterial.hh"
#include <gtest/gtest.h>

// TerrainSystem tests need AppContext infrastructure
#include "fabric/core/AppContext.hh"
#include "fabric/core/AssetRegistry.hh"
#include "fabric/core/ResourceHub.hh"
#include "fabric/core/SystemRegistry.hh"
#include "fabric/platform/ConfigManager.hh"
#include "recurse/systems/TerrainSystem.hh"

using namespace recurse::simulation;
using namespace recurse;
using fabric::K_CHUNK_SIZE;

// -- FlatWorldGenerator -------------------------------------------------------

TEST(TestWorldGenerator, FlatWorldGenerator_BelowGround_Stone) {
    SimulationGrid grid;
    FlatWorldGenerator gen(32);
    gen.generate(grid, 0, 0, 0);
    grid.advanceEpoch();

    // Chunk (0,0,0) spans y=[0,31], entirely below groundLevel=32
    for (int y = 0; y < K_CHUNK_SIZE; ++y) {
        auto cell = grid.readCell(0, y, 0);
        EXPECT_EQ(cell.materialId, material_ids::STONE) << "y=" << y << " should be stone";
    }
}

TEST(TestWorldGenerator, FlatWorldGenerator_AboveGround_Air) {
    SimulationGrid grid;
    FlatWorldGenerator gen(32);
    gen.generate(grid, 0, 2, 0); // y range [64, 95]
    grid.advanceEpoch();

    for (int y = 64; y < 96; ++y) {
        auto cell = grid.readCell(0, y, 0);
        EXPECT_EQ(cell.materialId, material_ids::AIR) << "y=" << y << " should be air";
    }
}

// -- LayeredWorldGenerator ----------------------------------------------------

TEST(TestWorldGenerator, LayeredWorldGenerator_StoneAndSand) {
    SimulationGrid grid;
    LayeredWorldGenerator gen(28, 4); // stone < 28, sand [28,31], air >= 32
    gen.generate(grid, 0, 0, 0);
    grid.advanceEpoch();

    // Stone region: y=[0,27]
    for (int y = 0; y < 28; ++y) {
        auto cell = grid.readCell(0, y, 0);
        EXPECT_EQ(cell.materialId, material_ids::STONE) << "y=" << y << " should be stone";
    }
    // Sand region: y=[28,31]
    for (int y = 28; y < 32; ++y) {
        auto cell = grid.readCell(0, y, 0);
        EXPECT_EQ(cell.materialId, material_ids::SAND) << "y=" << y << " should be sand";
    }
}

// -- FlatWorldGenerator sampleMaterial ----------------------------------------

TEST(TestWorldGenerator, FlatSampleMaterial_BelowGround_Stone) {
    FlatWorldGenerator gen(32);
    EXPECT_EQ(gen.sampleMaterial(0, 0, 0), material_ids::STONE);
    EXPECT_EQ(gen.sampleMaterial(5, 31, 10), material_ids::STONE);
}

TEST(TestWorldGenerator, FlatSampleMaterial_AtAndAboveGround_Air) {
    FlatWorldGenerator gen(32);
    EXPECT_EQ(gen.sampleMaterial(0, 32, 0), material_ids::AIR);
    EXPECT_EQ(gen.sampleMaterial(0, 100, 0), material_ids::AIR);
}

// -- LayeredWorldGenerator sampleMaterial -------------------------------------

TEST(TestWorldGenerator, LayeredSampleMaterial_StoneAndSandAndAir) {
    LayeredWorldGenerator gen(28, 4);
    EXPECT_EQ(gen.sampleMaterial(0, 0, 0), material_ids::STONE);
    EXPECT_EQ(gen.sampleMaterial(0, 27, 0), material_ids::STONE);
    EXPECT_EQ(gen.sampleMaterial(0, 28, 0), material_ids::SAND);
    EXPECT_EQ(gen.sampleMaterial(0, 31, 0), material_ids::SAND);
    EXPECT_EQ(gen.sampleMaterial(0, 32, 0), material_ids::AIR);
}

// -- Interface ----------------------------------------------------------------

TEST(TestWorldGenerator, WorldGeneratorInterface_Swappable) {
    SimulationGrid grid;

    // Generate with FlatWorldGenerator(32): below y=32 is stone
    FlatWorldGenerator flat(32);
    flat.generate(grid, 0, 0, 0);
    grid.advanceEpoch();
    EXPECT_EQ(grid.readCell(0, 28, 0).materialId, material_ids::STONE);

    // Re-generate same chunk with LayeredWorldGenerator(28, 4):
    // y=28 should now be sand instead of stone
    LayeredWorldGenerator layered(28, 4);
    layered.generate(grid, 0, 0, 0);
    grid.advanceEpoch();
    EXPECT_EQ(grid.readCell(0, 28, 0).materialId, material_ids::SAND);
}

TEST(TestWorldGenerator, GeneratorName_ReturnsCorrect) {
    FlatWorldGenerator flat;
    EXPECT_EQ(flat.name(), "FlatWorldGenerator");

    LayeredWorldGenerator layered;
    EXPECT_EQ(layered.name(), "LayeredWorldGenerator");
}

// -- TerrainSystem ------------------------------------------------------------

TEST(TestWorldGenerator, TerrainSystem_InitCreatesGrid) {
    fabric::World world;
    fabric::Timeline timeline;
    fabric::EventDispatcher dispatcher;
    fabric::ResourceHub hub;
    hub.disableWorkerThreadsForTesting();
    fabric::AssetRegistry assetRegistry{hub};
    fabric::SystemRegistry sysReg;
    fabric::ConfigManager configManager;

    sysReg.registerSystem<recurse::systems::TerrainSystem>(fabric::SystemPhase::FixedUpdate);
    ASSERT_TRUE(sysReg.resolve());

    fabric::AppContext ctx{
        .world = world,
        .timeline = timeline,
        .dispatcher = dispatcher,
        .resourceHub = hub,
        .assetRegistry = assetRegistry,
        .systemRegistry = sysReg,
        .configManager = configManager,
    };
    sysReg.initAll(ctx);

    auto* terrain = sysReg.get<recurse::systems::TerrainSystem>();
    ASSERT_NE(terrain, nullptr);
    // After init, worldGenerator should be valid
    EXPECT_NO_THROW(terrain->worldGenerator());
    EXPECT_EQ(terrain->worldGenerator().name(), "FlatWorldGenerator");

    sysReg.shutdownAll();
}
