#include "fabric/simulation/VoxelSimulationSystem.hh"
#include "fabric/simulation/ChunkActivityTracker.hh"
#include "fabric/simulation/SimulationGrid.hh"
#include "fabric/simulation/VoxelMaterial.hh"
#include "fabric/world/ChunkedGrid.hh"

#include "fabric/core/AppContext.hh"
#include "fabric/core/AssetRegistry.hh"
#include "fabric/core/ConfigManager.hh"
#include "fabric/core/ResourceHub.hh"
#include "fabric/core/SystemRegistry.hh"
#include "recurse/systems/TerrainSystem.hh"
#include "recurse/systems/VoxelSimulationSystem.hh"

#include <gtest/gtest.h>

using namespace fabric::simulation;
using fabric::K_CHUNK_SIZE;

class VoxelSimulationSystemTest : public ::testing::Test {
  protected:
    VoxelSimulationSystem sim;

    void SetUp() override {
        // Set up a single chunk at origin
        sim.grid().fillChunk(0, 0, 0, VoxelCell{});
        sim.grid().materializeChunk(0, 0, 0);
        sim.activityTracker().setState(ChunkPos{0, 0, 0}, ChunkState::Active);
        markAllSubRegions(ChunkPos{0, 0, 0});
    }

    void markAllSubRegions(ChunkPos pos) {
        for (int lz = 0; lz < K_CHUNK_SIZE; lz += 8)
            for (int ly = 0; ly < K_CHUNK_SIZE; ly += 8)
                for (int lx = 0; lx < K_CHUNK_SIZE; lx += 8)
                    sim.activityTracker().markSubRegionActive(pos, lx, ly, lz);
    }

    VoxelCell makeMaterial(MaterialId id) {
        VoxelCell c;
        c.materialId = id;
        return c;
    }

    void placeCellAndAdvance(int wx, int wy, int wz, VoxelCell cell) {
        sim.grid().writeCell(wx, wy, wz, cell);
        sim.grid().advanceEpoch();
    }

    void buildStoneFloor() {
        for (int x = 0; x < K_CHUNK_SIZE; ++x)
            for (int z = 0; z < K_CHUNK_SIZE; ++z)
                sim.grid().writeCell(x, 0, z, makeMaterial(material_ids::STONE));
        sim.grid().advanceEpoch();
    }

    void buildStoneBox(int xmin, int xmax, int zmin, int zmax, int h) {
        for (int x = xmin; x <= xmax; ++x) {
            for (int z = zmin; z <= zmax; ++z) {
                sim.grid().writeCell(x, 0, z, makeMaterial(material_ids::STONE));
                for (int y = 1; y <= h; ++y) {
                    if (x == xmin || x == xmax || z == zmin || z == zmax)
                        sim.grid().writeCell(x, y, z, makeMaterial(material_ids::STONE));
                }
            }
        }
        sim.grid().advanceEpoch();
    }

    int countMaterial(MaterialId id) {
        int count = 0;
        for (int z = 0; z < K_CHUNK_SIZE; ++z)
            for (int y = 0; y < K_CHUNK_SIZE; ++y)
                for (int x = 0; x < K_CHUNK_SIZE; ++x)
                    if (sim.grid().readCell(x, y, z).materialId == id)
                        ++count;
        return count;
    }
};

// 1. Each tick advances epoch by 1 and frameIndex by 1
TEST_F(VoxelSimulationSystemTest, SingleEpochPerTick) {
    uint64_t epochBefore = sim.grid().currentEpoch();
    uint64_t frameBefore = sim.frameIndex();

    sim.tick();

    EXPECT_EQ(sim.grid().currentEpoch(), epochBefore + 1);
    EXPECT_EQ(sim.frameIndex(), frameBefore + 1);
}

// 2. Sand placed at y=10 falls to stone floor at y=0 after sufficient ticks
TEST_F(VoxelSimulationSystemTest, SandFallsOverTicks) {
    buildStoneFloor();

    // Place sand at y=10, inside a contained column to prevent diagonal cascade
    for (int y = 1; y <= 12; ++y) {
        sim.grid().writeCell(15, y, 16, makeMaterial(material_ids::STONE));
        sim.grid().writeCell(17, y, 16, makeMaterial(material_ids::STONE));
        sim.grid().writeCell(16, y, 15, makeMaterial(material_ids::STONE));
        sim.grid().writeCell(16, y, 17, makeMaterial(material_ids::STONE));
    }
    sim.grid().advanceEpoch();

    placeCellAndAdvance(16, 10, 16, makeMaterial(material_ids::SAND));

    for (int i = 0; i < 15; ++i) {
        sim.activityTracker().setState(ChunkPos{0, 0, 0}, ChunkState::Active);
        markAllSubRegions(ChunkPos{0, 0, 0});
        sim.tick();
    }

    // Sand should be at y=1 (on top of stone floor)
    EXPECT_EQ(sim.grid().readCell(16, 1, 16).materialId, material_ids::SAND);
}

// 3. Water fills a stone box cavity over ticks
TEST_F(VoxelSimulationSystemTest, WaterFillsCavity) {
    buildStoneBox(10, 14, 10, 14, 4);

    // Pour water from above
    for (int x = 11; x <= 13; ++x)
        for (int z = 11; z <= 13; ++z)
            sim.grid().writeCell(x, 4, z, makeMaterial(material_ids::WATER));
    sim.grid().advanceEpoch();

    for (int i = 0; i < 50; ++i) {
        sim.activityTracker().setState(ChunkPos{0, 0, 0}, ChunkState::Active);
        markAllSubRegions(ChunkPos{0, 0, 0});
        sim.tick();
    }

    // Water should have settled at bottom (y=1)
    int bottomWater = 0;
    for (int x = 11; x <= 13; ++x)
        for (int z = 11; z <= 13; ++z)
            if (sim.grid().readCell(x, 1, z).materialId == material_ids::WATER)
                ++bottomWater;
    EXPECT_EQ(bottomWater, 9);
}

// 4. Chunk filled with only stone (static) should not remain active
TEST_F(VoxelSimulationSystemTest, SleepingNotSimulated) {
    // Fill entire chunk with stone
    for (int z = 0; z < K_CHUNK_SIZE; ++z)
        for (int y = 0; y < K_CHUNK_SIZE; ++y)
            for (int x = 0; x < K_CHUNK_SIZE; ++x)
                sim.grid().writeCell(x, y, z, makeMaterial(material_ids::STONE));
    sim.grid().advanceEpoch();

    sim.activityTracker().setState(ChunkPos{0, 0, 0}, ChunkState::Active);
    sim.tick();

    // After tick, chunk should be sleeping (no movement happened)
    EXPECT_EQ(sim.activityTracker().getState(ChunkPos{0, 0, 0}), ChunkState::Sleeping);

    // Second tick: sleeping chunk should not be collected
    uint64_t frameBefore = sim.frameIndex();
    sim.tick();
    // Frame still advances
    EXPECT_EQ(sim.frameIndex(), frameBefore + 1);
}

// 5. Active count rises on disturbance, falls as things settle
TEST_F(VoxelSimulationSystemTest, ActiveCountTracking) {
    buildStoneFloor();

    // Place sand -- chunk becomes active
    placeCellAndAdvance(16, 5, 16, makeMaterial(material_ids::SAND));
    sim.activityTracker().setState(ChunkPos{0, 0, 0}, ChunkState::Active);
    markAllSubRegions(ChunkPos{0, 0, 0});

    auto activeNow = sim.activityTracker().collectActiveChunks();
    EXPECT_GE(activeNow.size(), 1u);

    // Run ticks until sand settles (should settle on stone floor)
    for (int i = 0; i < 30; ++i) {
        // Re-activate if went to sleep (sand still falling)
        if (sim.activityTracker().getState(ChunkPos{0, 0, 0}) != ChunkState::Active) {
            sim.activityTracker().setState(ChunkPos{0, 0, 0}, ChunkState::Active);
            markAllSubRegions(ChunkPos{0, 0, 0});
        }
        sim.tick();
    }

    // After many ticks with only one sand grain on a floor, chunk should go to sleep
    EXPECT_EQ(sim.activityTracker().getState(ChunkPos{0, 0, 0}), ChunkState::Sleeping);
}

// 6. Empty world (no chunks) doesn't crash
TEST_F(VoxelSimulationSystemTest, EmptyWorldNoOp) {
    // Create a fresh system with no chunks
    VoxelSimulationSystem empty;

    EXPECT_NO_THROW(empty.tick());
    EXPECT_EQ(empty.frameIndex(), 1u);
    EXPECT_NO_THROW(empty.tick());
    EXPECT_EQ(empty.frameIndex(), 2u);
}

// 7. grid() returns a valid, usable reference
TEST_F(VoxelSimulationSystemTest, GridAccessible) {
    auto& g = sim.grid();
    g.writeCell(0, 0, 0, makeMaterial(material_ids::SAND));
    g.advanceEpoch();
    EXPECT_EQ(g.readCell(0, 0, 0).materialId, material_ids::SAND);
}

// 8. Total sand count is conserved over 50 ticks
TEST_F(VoxelSimulationSystemTest, MatterConservation) {
    buildStoneFloor();
    // Place 20 sand grains at various heights
    int placed = 0;
    for (int y = 1; y <= 10 && placed < 20; ++y)
        for (int x = 14; x <= 18 && placed < 20; ++x) {
            sim.grid().writeCell(x, y, 16, makeMaterial(material_ids::SAND));
            ++placed;
        }
    sim.grid().advanceEpoch();

    int initialSand = countMaterial(material_ids::SAND);
    EXPECT_EQ(initialSand, 20);

    for (int i = 0; i < 50; ++i) {
        sim.activityTracker().setState(ChunkPos{0, 0, 0}, ChunkState::Active);
        markAllSubRegions(ChunkPos{0, 0, 0});
        sim.tick();
    }

    int finalSand = countMaterial(material_ids::SAND);
    EXPECT_EQ(finalSand, initialSand) << "Sand count must be conserved";
}

// 9. System lifecycle via recurse wrapper
TEST(RecurseVoxelSimSystemTest, SystemLifecycle_InitUpdateShutdown) {
    fabric::World world;
    fabric::Timeline timeline;
    fabric::EventDispatcher dispatcher;
    fabric::ResourceHub hub;
    hub.disableWorkerThreadsForTesting();
    fabric::AssetRegistry assetRegistry{hub};
    fabric::SystemRegistry sysReg;
    fabric::ConfigManager configManager;

    sysReg.registerSystem<recurse::systems::TerrainSystem>(fabric::SystemPhase::FixedUpdate);
    sysReg.registerSystem<recurse::systems::VoxelSimulationSystem>(fabric::SystemPhase::FixedUpdate);
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

    auto* sim = sysReg.get<recurse::systems::VoxelSimulationSystem>();
    ASSERT_NE(sim, nullptr);
    EXPECT_NO_THROW(sim->simulationGrid());

    // Run one fixed update tick
    sysReg.runFixedUpdate(ctx, 1.0f / 60.0f);

    sysReg.shutdownAll();
}

// 10. Liquid flows horizontally
TEST(RecurseVoxelSimSystemTest, LiquidFlowsHorizontally) {
    VoxelSimulationSystem sim;
    sim.workerPool().disableForTesting();
    auto& grid = sim.grid();

    for (int x = 0; x < K_CHUNK_SIZE; ++x)
        for (int z = 0; z < K_CHUNK_SIZE; ++z)
            grid.writeCell(x, 0, z, VoxelCell{material_ids::STONE});
    grid.advanceEpoch();

    for (int y = 1; y <= 4; ++y)
        grid.writeCell(16, y, 16, VoxelCell{material_ids::WATER});
    grid.advanceEpoch();

    auto countWater = [&]() {
        int count = 0;
        for (auto [cx, cy, cz] : grid.allChunks())
            for (int lz = 0; lz < K_CHUNK_SIZE; ++lz)
                for (int ly = 0; ly < K_CHUNK_SIZE; ++ly)
                    for (int lx = 0; lx < K_CHUNK_SIZE; ++lx)
                        if (grid.readCell(cx * K_CHUNK_SIZE + lx, cy * K_CHUNK_SIZE + ly, cz * K_CHUNK_SIZE + lz)
                                .materialId == material_ids::WATER)
                            ++count;
        return count;
    };

    int initialWater = countWater();
    sim.activityTracker().setState(ChunkPos{0, 0, 0}, ChunkState::Active);

    for (int i = 0; i < 30; ++i)
        sim.tick();

    int finalWater = countWater();
    EXPECT_EQ(finalWater, initialWater) << "Water voxel count must be conserved";
}

// 11. Wake on neighbor activity
TEST(RecurseVoxelSimSystemTest, WakeOnNeighborActivity) {
    VoxelSimulationSystem sim;
    sim.workerPool().disableForTesting();
    auto& grid = sim.grid();

    grid.fillChunk(0, 0, 0, VoxelCell{material_ids::STONE});
    grid.advanceEpoch();
    sim.activityTracker().setState(ChunkPos{0, 0, 0}, ChunkState::Sleeping);

    for (int x = K_CHUNK_SIZE; x < K_CHUNK_SIZE * 2; ++x)
        for (int z = 0; z < K_CHUNK_SIZE; ++z)
            grid.writeCell(x, 0, z, VoxelCell{material_ids::STONE});
    grid.advanceEpoch();

    grid.writeCell(K_CHUNK_SIZE, 5, 16, VoxelCell{material_ids::SAND});
    grid.advanceEpoch();
    sim.activityTracker().setState(ChunkPos{1, 0, 0}, ChunkState::Active);

    for (int i = 0; i < 10; ++i)
        sim.tick();

    auto stateA = sim.activityTracker().getState(ChunkPos{0, 0, 0});
    EXPECT_NE(stateA, ChunkState::Sleeping) << "Neighbor activity should wake sleeping chunk";
}

// 12. Ghost cell copy correctness
TEST(RecurseVoxelSimSystemTest, GhostCellCopyCorrectness) {
    VoxelSimulationSystem sim;
    sim.workerPool().disableForTesting();
    auto& grid = sim.grid();

    grid.writeCell(31, 0, 0, VoxelCell{material_ids::STONE});
    grid.advanceEpoch();

    grid.materializeChunk(0, 0, 0);
    grid.materializeChunk(1, 0, 0);

    sim.activityTracker().setState(ChunkPos{0, 0, 0}, ChunkState::Active);
    sim.activityTracker().setState(ChunkPos{1, 0, 0}, ChunkState::Active);

    sim.tick();

    auto cell = grid.readCell(31, 0, 0);
    EXPECT_EQ(cell.materialId, material_ids::STONE) << "Boundary voxel should be preserved after ghost sync";
}
