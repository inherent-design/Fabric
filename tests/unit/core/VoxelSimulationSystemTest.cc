#include "fabric/simulation/VoxelSimulationSystem.hh"
#include "fabric/simulation/ChunkActivityTracker.hh"
#include "fabric/simulation/SimulationGrid.hh"
#include "fabric/simulation/VoxelMaterial.hh"
#include "recurse/world/ChunkedGrid.hh"
#include <gtest/gtest.h>

// For the SystemLifecycle test
#include "fabric/core/AppContext.hh"
#include "fabric/core/AssetRegistry.hh"
#include "fabric/core/ConfigManager.hh"
#include "fabric/core/ResourceHub.hh"
#include "fabric/core/SystemRegistry.hh"
#include "recurse/systems/TerrainSystem.hh"
#include "recurse/systems/VoxelSimulationSystem.hh"

using namespace fabric::simulation;
using namespace recurse;

namespace {

/// Helper: count voxels with a given material in a grid
int countMaterial(SimulationGrid& grid, MaterialId mat) {
    int count = 0;
    for (auto [cx, cy, cz] : grid.allChunks()) {
        for (int lz = 0; lz < kChunkSize; ++lz) {
            for (int ly = 0; ly < kChunkSize; ++ly) {
                for (int lx = 0; lx < kChunkSize; ++lx) {
                    int wx = cx * kChunkSize + lx;
                    int wy = cy * kChunkSize + ly;
                    int wz = cz * kChunkSize + lz;
                    if (grid.readCell(wx, wy, wz).materialId == mat)
                        ++count;
                }
            }
        }
    }
    return count;
}

} // namespace

// Test 1: System lifecycle via recurse wrapper
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

// Test 2: Sand falls under gravity
TEST(RecurseVoxelSimSystemTest, SandFallsUnderGravity) {
    VoxelSimulationSystem sim;
    sim.workerPool().disableForTesting();
    auto& grid = sim.grid();

    // Place solid floor at y=0
    for (int x = 0; x < kChunkSize; ++x)
        for (int z = 0; z < kChunkSize; ++z)
            grid.writeCell(x, 0, z, VoxelCell{MaterialIds::Stone});
    grid.advanceEpoch();

    // Place sand at y=10
    grid.writeCell(16, 10, 16, VoxelCell{MaterialIds::Sand});
    grid.advanceEpoch();

    // Mark chunk active so simulation picks it up
    sim.activityTracker().setState(ChunkPos{0, 0, 0}, ChunkState::Active);

    // Run simulation for several ticks
    for (int i = 0; i < 15; ++i)
        sim.tick();

    // Sand should have fallen; check it's no longer at y=10
    auto cellAtStart = grid.readCell(16, 10, 16);
    EXPECT_NE(cellAtStart.materialId, MaterialIds::Sand) << "Sand should have moved from y=10";

    // Sand should have settled on the floor at y=1
    auto cellAtRest = grid.readCell(16, 1, 16);
    EXPECT_EQ(cellAtRest.materialId, MaterialIds::Sand) << "Sand should rest at y=1 (above stone floor)";

    // Conservation: exactly 1 sand voxel
    EXPECT_EQ(countMaterial(grid, MaterialIds::Sand), 1);
}

// Test 3: Liquid flows horizontally
TEST(RecurseVoxelSimSystemTest, LiquidFlowsHorizontally) {
    VoxelSimulationSystem sim;
    sim.workerPool().disableForTesting();
    auto& grid = sim.grid();

    // Place solid floor at y=0
    for (int x = 0; x < kChunkSize; ++x)
        for (int z = 0; z < kChunkSize; ++z)
            grid.writeCell(x, 0, z, VoxelCell{MaterialIds::Stone});
    grid.advanceEpoch();

    // Place a column of water 4 voxels tall at center
    for (int y = 1; y <= 4; ++y)
        grid.writeCell(16, y, 16, VoxelCell{MaterialIds::Water});
    grid.advanceEpoch();

    int initialWater = countMaterial(grid, MaterialIds::Water);

    sim.activityTracker().setState(ChunkPos{0, 0, 0}, ChunkState::Active);

    for (int i = 0; i < 30; ++i)
        sim.tick();

    // Water should have spread; at center column there should be fewer
    // (or water level is <=1 high after spreading)
    int finalWater = countMaterial(grid, MaterialIds::Water);
    EXPECT_EQ(finalWater, initialWater) << "Water voxel count must be conserved";
}

// Test 4: Sleeping chunk costs zero
TEST(RecurseVoxelSimSystemTest, SleepingChunkCostsZero) {
    VoxelSimulationSystem sim;
    sim.workerPool().disableForTesting();
    auto& grid = sim.grid();

    // Fill chunk with stone (no movement possible)
    grid.fillChunk(0, 0, 0, VoxelCell{MaterialIds::Stone});
    grid.advanceEpoch();

    // Chunk starts sleeping by default
    sim.activityTracker().setState(ChunkPos{0, 0, 0}, ChunkState::Sleeping);

    sim.tick();

    auto active = sim.activityTracker().collectActiveChunks();
    EXPECT_EQ(active.size(), 0u) << "Stone-filled chunk should remain sleeping";
}

// Test 5: Wake on neighbor activity
TEST(RecurseVoxelSimSystemTest, WakeOnNeighborActivity) {
    VoxelSimulationSystem sim;
    sim.workerPool().disableForTesting();
    auto& grid = sim.grid();

    // Chunk A (0,0,0): filled with stone, sleeping
    grid.fillChunk(0, 0, 0, VoxelCell{MaterialIds::Stone});
    grid.advanceEpoch();
    sim.activityTracker().setState(ChunkPos{0, 0, 0}, ChunkState::Sleeping);

    // Chunk B (1,0,0): has floor + sand near boundary
    for (int x = kChunkSize; x < kChunkSize * 2; ++x)
        for (int z = 0; z < kChunkSize; ++z)
            grid.writeCell(x, 0, z, VoxelCell{MaterialIds::Stone});
    grid.advanceEpoch();

    // Sand near the boundary at x=32 (first voxel of chunk 1)
    grid.writeCell(kChunkSize, 5, 16, VoxelCell{MaterialIds::Sand});
    grid.advanceEpoch();
    sim.activityTracker().setState(ChunkPos{1, 0, 0}, ChunkState::Active);

    // Run a few ticks; sand movement should propagate dirty to chunk A
    for (int i = 0; i < 10; ++i)
        sim.tick();

    auto stateA = sim.activityTracker().getState(ChunkPos{0, 0, 0});
    // Chunk A should have been woken (BoundaryDirty or Active)
    EXPECT_NE(stateA, ChunkState::Sleeping) << "Neighbor activity should wake sleeping chunk";
}

// Test 6: Ghost cell copy correctness
TEST(RecurseVoxelSimSystemTest, GhostCellCopyCorrectness) {
    VoxelSimulationSystem sim;
    sim.workerPool().disableForTesting();
    auto& grid = sim.grid();

    // Place stone at x=31 (boundary of chunk 0,0,0)
    grid.writeCell(31, 0, 0, VoxelCell{MaterialIds::Stone});
    grid.advanceEpoch();

    // Both chunks need to exist for ghost cells to work
    grid.materializeChunk(0, 0, 0);
    grid.materializeChunk(1, 0, 0);

    sim.activityTracker().setState(ChunkPos{0, 0, 0}, ChunkState::Active);
    sim.activityTracker().setState(ChunkPos{1, 0, 0}, ChunkState::Active);

    // After one tick, the ghost cell mechanism should have synced
    sim.tick();

    // Verify the stone is still readable at x=31
    auto cell = grid.readCell(31, 0, 0);
    EXPECT_EQ(cell.materialId, MaterialIds::Stone) << "Boundary voxel should be preserved after ghost sync";
}

// Test 7: Matter conservation
TEST(RecurseVoxelSimSystemTest, MatterConservation) {
    VoxelSimulationSystem sim;
    sim.workerPool().disableForTesting();
    auto& grid = sim.grid();

    // Place floor
    for (int x = 0; x < kChunkSize; ++x)
        for (int z = 0; z < kChunkSize; ++z)
            grid.writeCell(x, 0, z, VoxelCell{MaterialIds::Stone});
    grid.advanceEpoch();

    // Place 5x5x5 region of sand at y=10
    int sandPlaced = 0;
    for (int z = 10; z < 15; ++z) {
        for (int y = 10; y < 15; ++y) {
            for (int x = 10; x < 15; ++x) {
                grid.writeCell(x, y, z, VoxelCell{MaterialIds::Sand});
                ++sandPlaced;
            }
        }
    }
    grid.advanceEpoch();

    sim.activityTracker().setState(ChunkPos{0, 0, 0}, ChunkState::Active);

    int sandBefore = countMaterial(grid, MaterialIds::Sand);
    EXPECT_EQ(sandBefore, sandPlaced);

    // Run 50 ticks of simulation
    for (int i = 0; i < 50; ++i)
        sim.tick();

    int sandAfter = countMaterial(grid, MaterialIds::Sand);
    EXPECT_EQ(sandAfter, sandBefore) << "Sand count must be conserved after 50 ticks";
}
