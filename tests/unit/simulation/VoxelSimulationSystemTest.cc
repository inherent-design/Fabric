#include "recurse/simulation/VoxelSimulationSystem.hh"
#include "fabric/world/ChunkedGrid.hh"
#include "recurse/character/VoxelInteraction.hh"
#include "recurse/simulation/CellAccessors.hh"
#include "recurse/simulation/ChunkActivityTracker.hh"
#include "recurse/simulation/SimulationGrid.hh"
#include "recurse/simulation/VoxelMaterial.hh"

#include "fabric/core/AppContext.hh"
#include "fabric/core/SystemRegistry.hh"
#include "fabric/platform/ConfigManager.hh"
#include "fabric/resource/AssetRegistry.hh"
#include "fabric/resource/ResourceHub.hh"
#include "recurse/systems/TerrainSystem.hh"
#include "recurse/systems/VoxelSimulationSystem.hh"

#include <gtest/gtest.h>

using namespace recurse::simulation;

class VoxelSimulationSystemTest : public ::testing::Test {
  protected:
    VoxelSimulationSystem sim;

    void SetUp() override {
        // Set up a single chunk at origin
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

    VoxelCell makeMaterial(MaterialId id) { return cellForMaterial(id); }

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
                    if (cellMaterialId(sim.grid().readCell(x, y, z)) == id)
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
        sim.activityTracker().setState(ChunkCoord{0, 0, 0}, ChunkState::Active);
        markAllSubRegions(ChunkCoord{0, 0, 0});
        sim.tick();
    }

    // Sand should be at y=1 (on top of stone floor)
    EXPECT_EQ(cellMaterialId(sim.grid().readCell(16, 1, 16)), material_ids::SAND);
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
        sim.activityTracker().setState(ChunkCoord{0, 0, 0}, ChunkState::Active);
        markAllSubRegions(ChunkCoord{0, 0, 0});
        sim.tick();
    }

    // Water should have settled at bottom (y=1)
    int bottomWater = 0;
    for (int x = 11; x <= 13; ++x)
        for (int z = 11; z <= 13; ++z)
            if (cellMaterialId(sim.grid().readCell(x, 1, z)) == material_ids::WATER)
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

    sim.activityTracker().setState(ChunkCoord{0, 0, 0}, ChunkState::Active);
    sim.tick();

    // After tick, chunk should be sleeping (no movement happened)
    EXPECT_EQ(sim.activityTracker().getState(ChunkCoord{0, 0, 0}), ChunkState::Sleeping);

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
    sim.activityTracker().setState(ChunkCoord{0, 0, 0}, ChunkState::Active);
    markAllSubRegions(ChunkCoord{0, 0, 0});

    auto activeNow = sim.activityTracker().collectActiveChunks();
    EXPECT_GE(activeNow.size(), 1u);

    // Run ticks until sand settles (should settle on stone floor)
    for (int i = 0; i < 30; ++i) {
        // Re-activate if went to sleep (sand still falling)
        if (sim.activityTracker().getState(ChunkCoord{0, 0, 0}) != ChunkState::Active) {
            sim.activityTracker().setState(ChunkCoord{0, 0, 0}, ChunkState::Active);
            markAllSubRegions(ChunkCoord{0, 0, 0});
        }
        sim.tick();
    }

    // After many ticks with only one sand grain on a floor, chunk should go to sleep
    EXPECT_EQ(sim.activityTracker().getState(ChunkCoord{0, 0, 0}), ChunkState::Sleeping);
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
    EXPECT_EQ(cellMaterialId(g.readCell(0, 0, 0)), material_ids::SAND);
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
        sim.activityTracker().setState(ChunkCoord{0, 0, 0}, ChunkState::Active);
        markAllSubRegions(ChunkCoord{0, 0, 0});
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
    sim.scheduler().disableForTesting();
    auto& grid = sim.grid();

    for (int x = 0; x < K_CHUNK_SIZE; ++x)
        for (int z = 0; z < K_CHUNK_SIZE; ++z)
            grid.writeCell(x, 0, z, cellForMaterial(material_ids::STONE));
    grid.advanceEpoch();

    for (int y = 1; y <= 4; ++y)
        grid.writeCell(16, y, 16, cellForMaterial(material_ids::WATER));
    grid.advanceEpoch();

    auto countWater = [&]() {
        int count = 0;
        for (auto [cx, cy, cz] : grid.allChunks())
            for (int lz = 0; lz < K_CHUNK_SIZE; ++lz)
                for (int ly = 0; ly < K_CHUNK_SIZE; ++ly)
                    for (int lx = 0; lx < K_CHUNK_SIZE; ++lx)
                        if (cellMaterialId(grid.readCell(cx * K_CHUNK_SIZE + lx, cy * K_CHUNK_SIZE + ly,
                                                         cz * K_CHUNK_SIZE + lz)) == material_ids::WATER)
                            ++count;
        return count;
    };

    int initialWater = countWater();
    sim.activityTracker().setState(ChunkCoord{0, 0, 0}, ChunkState::Active);

    for (int i = 0; i < 30; ++i)
        sim.tick();

    int finalWater = countWater();
    EXPECT_EQ(finalWater, initialWater) << "Water voxel count must be conserved";
}

// 11. Wake on neighbor activity
TEST(RecurseVoxelSimSystemTest, WakeOnNeighborActivity) {
    VoxelSimulationSystem sim;
    sim.scheduler().disableForTesting();
    auto& grid = sim.grid();

    grid.fillChunk(0, 0, 0, cellForMaterial(material_ids::STONE));
    grid.advanceEpoch();
    sim.activityTracker().setState(ChunkCoord{0, 0, 0}, ChunkState::Sleeping);

    for (int x = K_CHUNK_SIZE; x < K_CHUNK_SIZE * 2; ++x)
        for (int z = 0; z < K_CHUNK_SIZE; ++z)
            grid.writeCell(x, 0, z, cellForMaterial(material_ids::STONE));
    grid.advanceEpoch();

    grid.writeCell(K_CHUNK_SIZE, 5, 16, cellForMaterial(material_ids::SAND));
    grid.advanceEpoch();
    sim.activityTracker().setState(ChunkCoord{1, 0, 0}, ChunkState::Active);

    for (int i = 0; i < 10; ++i)
        sim.tick();

    auto stateA = sim.activityTracker().getState(ChunkCoord{0, 0, 0});
    EXPECT_NE(stateA, ChunkState::Sleeping) << "Neighbor activity should wake sleeping chunk";
}

// 12. Ghost cell copy correctness
TEST(RecurseVoxelSimSystemTest, GhostCellCopyCorrectness) {
    VoxelSimulationSystem sim;
    sim.scheduler().disableForTesting();
    auto& grid = sim.grid();

    grid.writeCell(31, 0, 0, cellForMaterial(material_ids::STONE));
    grid.advanceEpoch();

    grid.materializeChunk(0, 0, 0);
    grid.materializeChunk(1, 0, 0);

    sim.activityTracker().setState(ChunkCoord{0, 0, 0}, ChunkState::Active);
    sim.activityTracker().setState(ChunkCoord{1, 0, 0}, ChunkState::Active);

    sim.tick();

    auto cell = grid.readCell(31, 0, 0);
    EXPECT_EQ(cellMaterialId(cell), material_ids::STONE) << "Boundary voxel should be preserved after ghost sync";
}

TEST(RecurseVoxelSimSystemTest, ApplyExternalEditIntoSleepingChunkWritesAndSimulatesWithoutCallerRepair) {
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
    sim->scheduler().disableForTesting();

    auto& grid = sim->simulationGrid();
    grid.fillChunk(0, 0, 0, VoxelCell{});
    grid.materializeChunk(0, 0, 0);
    for (int x = 0; x < K_CHUNK_SIZE; ++x)
        for (int z = 0; z < K_CHUNK_SIZE; ++z)
            grid.writeCellImmediate(x, 0, z, cellForMaterial(material_ids::STONE));

    sim->activityTracker().setState(ChunkCoord{0, 0, 0}, ChunkState::Sleeping);

    VoxelCell air{};
    VoxelCell sand = cellForMaterial(material_ids::SAND);
    sand.setFlags(voxel_flags::UPDATED);
    uint32_t oldCell = 0;
    uint32_t newCell = 0;
    std::memcpy(&oldCell, &air, sizeof(uint32_t));
    std::memcpy(&newCell, &sand, sizeof(uint32_t));

    int eventCount = 0;
    std::vector<recurse::VoxelChangeDetail> details;
    recurse::WorldChangeEnvelope envelope;
    auto listenerId = dispatcher.addEventListener(recurse::K_VOXEL_CHANGED_EVENT, [&](fabric::Event& event) {
        ++eventCount;
        EXPECT_EQ(event.getData<int>("cx"), 0);
        EXPECT_EQ(event.getData<int>("cy"), 0);
        EXPECT_EQ(event.getData<int>("cz"), 0);
        details = event.getAnyData<std::vector<recurse::VoxelChangeDetail>>("detail");
        envelope = event.getAnyData<recurse::WorldChangeEnvelope>(recurse::K_WORLD_CHANGE_ENVELOPE_KEY);
    });

    recurse::InteractionResult edit{true, 16, 10, 16, 0, 0, 0, sand, recurse::ChangeSource::Place, 0};
    sim->applyExternalEdit(edit);

    EXPECT_EQ(sim->activityTracker().getState(ChunkCoord{0, 0, 0}), ChunkState::Active);
    EXPECT_EQ(cellMaterialId(grid.readCell(16, 10, 16)), material_ids::SAND);
    ASSERT_EQ(eventCount, 1);
    ASSERT_EQ(details.size(), 1u);
    EXPECT_EQ(details[0].vx, 16);
    EXPECT_EQ(details[0].vy, 10);
    EXPECT_EQ(details[0].vz, 16);
    EXPECT_EQ(details[0].oldCell, oldCell);
    EXPECT_EQ(details[0].newCell, newCell);
    EXPECT_EQ(details[0].source, recurse::ChangeSource::Place);
    EXPECT_EQ(envelope.source, recurse::ChangeSource::Place);
    EXPECT_EQ(envelope.targetKind, recurse::FunctionTargetKind::Voxel);
    EXPECT_EQ(envelope.historyMode, recurse::FunctionHistoryMode::PerVoxelDelta);
    ASSERT_EQ(envelope.touchedChunks.size(), 1u);
    EXPECT_EQ(envelope.touchedChunks[0], (ChunkCoord{0, 0, 0}));
    ASSERT_EQ(envelope.voxelDeltas.size(), 1u);
    EXPECT_EQ(envelope.voxelDeltas[0].chunk, (ChunkCoord{0, 0, 0}));
    EXPECT_EQ(envelope.voxelDeltas[0].oldCell, oldCell);
    EXPECT_EQ(envelope.voxelDeltas[0].newCell, newCell);

    dispatcher.removeEventListener(recurse::K_VOXEL_CHANGED_EVENT, listenerId);

    sysReg.runFixedUpdate(ctx, 1.0f / 60.0f);

    EXPECT_EQ(cellMaterialId(grid.readCell(16, 9, 16)), material_ids::SAND);
    EXPECT_EQ(cellMaterialId(grid.readCell(16, 10, 16)), material_ids::AIR);

    sysReg.shutdownAll();
}
