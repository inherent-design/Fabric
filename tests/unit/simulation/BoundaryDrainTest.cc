#include "recurse/simulation/VoxelSimulationSystem.hh"
#include <gtest/gtest.h>

using namespace recurse::simulation;

class BoundaryDrainTest : public ::testing::Test {
  protected:
    VoxelCell makeMaterial(MaterialId id) {
        VoxelCell c;
        c.materialId = id;
        return c;
    }

    void markAllSubRegions(ChunkActivityTracker& tracker, ChunkCoord pos) {
        for (int lz = 0; lz < K_CHUNK_SIZE; lz += 8)
            for (int ly = 0; ly < K_CHUNK_SIZE; ly += 8)
                for (int lx = 0; lx < K_CHUNK_SIZE; lx += 8)
                    tracker.markSubRegionActive(pos, lx, ly, lz);
    }

    void activate(VoxelSimulationSystem& sim, ChunkCoord pos) {
        sim.activityTracker().setState(pos, ChunkState::Active);
        markAllSubRegions(sim.activityTracker(), pos);
    }

    int countNonAir(const SimulationGrid& grid, int cx, int cy, int cz) {
        int count = 0;
        int bx = cx * K_CHUNK_SIZE;
        int by = cy * K_CHUNK_SIZE;
        int bz = cz * K_CHUNK_SIZE;
        for (int lz = 0; lz < K_CHUNK_SIZE; ++lz)
            for (int ly = 0; ly < K_CHUNK_SIZE; ++ly)
                for (int lx = 0; lx < K_CHUNK_SIZE; ++lx)
                    if (grid.readCell(bx + lx, by + ly, bz + lz).materialId != material_ids::AIR)
                        ++count;
        return count;
    }

    std::vector<VoxelCell> snapshotChunk(const SimulationGrid& grid, int cx, int cy, int cz) {
        std::vector<VoxelCell> result(K_CHUNK_VOLUME);
        int bx = cx * K_CHUNK_SIZE;
        int by = cy * K_CHUNK_SIZE;
        int bz = cz * K_CHUNK_SIZE;
        for (int lz = 0; lz < K_CHUNK_SIZE; ++lz)
            for (int ly = 0; ly < K_CHUNK_SIZE; ++ly)
                for (int lx = 0; lx < K_CHUNK_SIZE; ++lx) {
                    int idx = lx + ly * K_CHUNK_SIZE + lz * K_CHUNK_SIZE * K_CHUNK_SIZE;
                    result[idx] = grid.readCell(bx + lx, by + ly, bz + lz);
                }
        return result;
    }
};

// Sorted drain produces identical results across two separate simulation runs
// (same initial state -> same final state, independent of scheduling).
TEST_F(BoundaryDrainTest, SortedDrainDeterministic) {
    auto runOnce = [&]() {
        VoxelSimulationSystem sim;
        sim.scheduler().disableForTesting();
        sim.setWorldSeed(42);

        // Two vertically-stacked chunks. Sand near Y boundary crosses down.
        ChunkCoord upper{0, 1, 0};
        ChunkCoord lower{0, 0, 0};

        sim.grid().fillChunk(upper.x, upper.y, upper.z, VoxelCell{});
        sim.grid().materializeChunk(upper.x, upper.y, upper.z);
        sim.grid().fillChunk(lower.x, lower.y, lower.z, VoxelCell{});
        sim.grid().materializeChunk(lower.x, lower.y, lower.z);

        // Stone floor at world y=0 in lower chunk
        int lowerBaseY = lower.y * K_CHUNK_SIZE;
        for (int x = 0; x < K_CHUNK_SIZE; ++x)
            for (int z = 0; z < K_CHUNK_SIZE; ++z)
                sim.grid().writeCell(x, lowerBaseY, z, makeMaterial(material_ids::STONE));

        // Sand at bottom of upper chunk (local y=0 -> will cross to lower chunk)
        int upperBaseY = upper.y * K_CHUNK_SIZE;
        sim.grid().writeCell(8, upperBaseY, 8, makeMaterial(material_ids::SAND));
        sim.grid().writeCell(16, upperBaseY, 16, makeMaterial(material_ids::SAND));
        sim.grid().writeCell(24, upperBaseY, 8, makeMaterial(material_ids::SAND));

        sim.grid().advanceEpoch();

        for (int tick = 0; tick < 5; ++tick) {
            activate(sim, upper);
            activate(sim, lower);
            sim.tick();
        }

        std::vector<std::vector<VoxelCell>> snapshots;
        snapshots.push_back(snapshotChunk(sim.grid(), upper.x, upper.y, upper.z));
        snapshots.push_back(snapshotChunk(sim.grid(), lower.x, lower.y, lower.z));
        return snapshots;
    };

    auto run1 = runOnce();
    auto run2 = runOnce();

    for (size_t chunk = 0; chunk < run1.size(); ++chunk)
        for (size_t i = 0; i < run1[chunk].size(); ++i)
            EXPECT_EQ(run1[chunk][i].materialId, run2[chunk][i].materialId)
                << "Mismatch in chunk " << chunk << " at index " << i;
}

// When two boundary writes target the same destination, material is conserved.
// The sorted-first writer wins; the loser's source cell is restored.
TEST_F(BoundaryDrainTest, BoundaryConflictConservesMaterial) {
    VoxelSimulationSystem sim;
    sim.scheduler().disableForTesting();
    sim.setWorldSeed(123);

    // Four chunks forming an L-shape around the conflict target:
    //   (0,1,0) and (1,1,0) are upper chunks (source of sand)
    //   (0,0,0) and (1,0,0) are lower chunks (target area)
    ChunkCoord c00{0, 0, 0};
    ChunkCoord c10{1, 0, 0};
    ChunkCoord c01{0, 1, 0};
    ChunkCoord c11{1, 1, 0};

    for (auto pos : {c00, c10, c01, c11}) {
        sim.grid().fillChunk(pos.x, pos.y, pos.z, VoxelCell{});
        sim.grid().materializeChunk(pos.x, pos.y, pos.z);
    }

    // Stone floor at y=30 in chunk (1,0,0) so sand rests at y=31
    // (y=30 is local y=30, world y=30)
    for (int x = 32; x < 64; ++x)
        for (int z = 0; z < K_CHUNK_SIZE; ++z)
            sim.grid().writeCell(x, 30, z, makeMaterial(material_ids::STONE));

    // Block positions to force chunk (0,1,0) sand into the +X diagonal.
    // Sand at chunk (0,1,0) local (31,0,16) = world (31,32,16).
    // Direct fall: world (31,31,16) -> block with stone.
    // Diag (-1,-1,0): world (30,31,16) -> block with stone.
    // Diag (0,-1,-1): world (31,31,15) -> block with stone.
    // Diag (0,-1,+1): world (31,31,17) -> block with stone.
    // Only diag (+1,-1,0): world (32,31,16) remains open -> conflict target.
    sim.grid().writeCell(31, 31, 16, makeMaterial(material_ids::STONE));
    sim.grid().writeCell(30, 31, 16, makeMaterial(material_ids::STONE));
    sim.grid().writeCell(31, 31, 15, makeMaterial(material_ids::STONE));
    sim.grid().writeCell(31, 31, 17, makeMaterial(material_ids::STONE));

    // Place sand in chunk (0,1,0) at local (31,0,16) = world (31,32,16)
    sim.grid().writeCell(31, 32, 16, makeMaterial(material_ids::SAND));

    // Place sand in chunk (1,1,0) at local (0,0,16) = world (32,32,16)
    // Direct fall: world (32,31,16) -> the conflict target (air)
    sim.grid().writeCell(32, 32, 16, makeMaterial(material_ids::SAND));

    sim.grid().advanceEpoch();

    // Count non-air cells before tick
    int countBefore = 0;
    for (auto pos : {c00, c10, c01, c11})
        countBefore += countNonAir(sim.grid(), pos.x, pos.y, pos.z);

    // Run one simulation tick
    for (auto pos : {c00, c10, c01, c11})
        activate(sim, pos);
    sim.tick();

    // Count non-air cells after tick
    int countAfter = 0;
    for (auto pos : {c00, c10, c01, c11})
        countAfter += countNonAir(sim.grid(), pos.x, pos.y, pos.z);

    EXPECT_EQ(countBefore, countAfter) << "Material not conserved: before=" << countBefore << " after=" << countAfter;
}

// Standard single-grain boundary crossing still works with the sorted drain.
TEST_F(BoundaryDrainTest, SortedDrainNormalCaseUnchanged) {
    VoxelSimulationSystem sim;
    sim.scheduler().disableForTesting();
    sim.setWorldSeed(7);

    ChunkCoord upper{0, 1, 0};
    ChunkCoord lower{0, 0, 0};

    sim.grid().fillChunk(upper.x, upper.y, upper.z, VoxelCell{});
    sim.grid().materializeChunk(upper.x, upper.y, upper.z);
    sim.grid().fillChunk(lower.x, lower.y, lower.z, VoxelCell{});
    sim.grid().materializeChunk(lower.x, lower.y, lower.z);

    // Stone floor at world y=30 in lower chunk
    for (int x = 0; x < K_CHUNK_SIZE; ++x)
        for (int z = 0; z < K_CHUNK_SIZE; ++z)
            sim.grid().writeCell(x, 30, z, makeMaterial(material_ids::STONE));

    // Sand at bottom of upper chunk: local (16,0,16) = world (16,32,16)
    // Falls to world (16,31,16) which is in lower chunk local (16,31,16)
    sim.grid().writeCell(16, 32, 16, makeMaterial(material_ids::SAND));
    sim.grid().advanceEpoch();

    for (int tick = 0; tick < 3; ++tick) {
        activate(sim, upper);
        activate(sim, lower);
        sim.tick();
    }

    // Sand should have crossed the boundary and be at y=31 in lower chunk
    EXPECT_EQ(sim.grid().readCell(16, 31, 16).materialId, material_ids::SAND);
    EXPECT_EQ(sim.grid().readCell(16, 32, 16).materialId, material_ids::AIR);
}
