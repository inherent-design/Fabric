#include "fabric/simulation/VoxelSimulationSystem.hh"
#include <gtest/gtest.h>

using namespace fabric::simulation;
using recurse::kChunkSize;

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
        for (int lz = 0; lz < kChunkSize; lz += 8)
            for (int ly = 0; ly < kChunkSize; ly += 8)
                for (int lx = 0; lx < kChunkSize; lx += 8)
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
        for (int x = 0; x < kChunkSize; ++x)
            for (int z = 0; z < kChunkSize; ++z)
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
        for (int z = 0; z < kChunkSize; ++z)
            for (int y = 0; y < kChunkSize; ++y)
                for (int x = 0; x < kChunkSize; ++x)
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
    for (int z = 0; z < kChunkSize; ++z)
        for (int y = 0; y < kChunkSize; ++y)
            for (int x = 0; x < kChunkSize; ++x)
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
