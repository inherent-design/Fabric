#include "recurse/world/TestWorldGenerator.hh"
#include "fabric/simulation/SimulationGrid.hh"
#include "fabric/simulation/VoxelMaterial.hh"
#include "recurse/world/ChunkedGrid.hh"
#include <gtest/gtest.h>

// TerrainSystem tests need AppContext infrastructure
#include "fabric/core/AppContext.hh"
#include "fabric/core/AssetRegistry.hh"
#include "fabric/core/ConfigManager.hh"
#include "fabric/core/ResourceHub.hh"
#include "fabric/core/SystemRegistry.hh"
#include "recurse/systems/TerrainSystem.hh"

using namespace fabric::simulation;
using namespace recurse;

// -- FlatWorldGenerator -------------------------------------------------------

TEST(TestWorldGenerator, FlatWorldGenerator_BelowGround_Stone) {
    SimulationGrid grid;
    FlatWorldGenerator gen(32);
    gen.generate(grid, 0, 0, 0);
    grid.advanceEpoch();

    // Chunk (0,0,0) spans y=[0,31], entirely below groundLevel=32
    for (int y = 0; y < kChunkSize; ++y) {
        auto cell = grid.readCell(0, y, 0);
        EXPECT_EQ(cell.materialId, MaterialIds::Stone) << "y=" << y << " should be stone";
    }
}

TEST(TestWorldGenerator, FlatWorldGenerator_AboveGround_Air) {
    SimulationGrid grid;
    FlatWorldGenerator gen(32);
    gen.generate(grid, 0, 2, 0); // y range [64, 95]
    grid.advanceEpoch();

    for (int y = 64; y < 96; ++y) {
        auto cell = grid.readCell(0, y, 0);
        EXPECT_EQ(cell.materialId, MaterialIds::Air) << "y=" << y << " should be air";
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
        EXPECT_EQ(cell.materialId, MaterialIds::Stone) << "y=" << y << " should be stone";
    }
    // Sand region: y=[28,31]
    for (int y = 28; y < 32; ++y) {
        auto cell = grid.readCell(0, y, 0);
        EXPECT_EQ(cell.materialId, MaterialIds::Sand) << "y=" << y << " should be sand";
    }
}

// -- Interface ----------------------------------------------------------------

TEST(TestWorldGenerator, WorldGeneratorInterface_Swappable) {
    SimulationGrid grid;

    // Generate with FlatWorldGenerator(32): below y=32 is stone
    FlatWorldGenerator flat(32);
    flat.generate(grid, 0, 0, 0);
    grid.advanceEpoch();
    EXPECT_EQ(grid.readCell(0, 28, 0).materialId, MaterialIds::Stone);

    // Re-generate same chunk with LayeredWorldGenerator(28, 4):
    // y=28 should now be sand instead of stone
    LayeredWorldGenerator layered(28, 4);
    layered.generate(grid, 0, 0, 0);
    grid.advanceEpoch();
    EXPECT_EQ(grid.readCell(0, 28, 0).materialId, MaterialIds::Sand);
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
    // After init, simulationGrid and worldGenerator should be valid
    EXPECT_NO_THROW(terrain->simulationGrid());
    EXPECT_NO_THROW(terrain->worldGenerator());
    EXPECT_EQ(terrain->worldGenerator().name(), "FlatWorldGenerator");

    sysReg.shutdownAll();
}
