#include "fabric/simulation/SimWorkerPool.hh"
#include "fabric/simulation/VoxelSimulationSystem.hh"
#include <chrono>
#include <gtest/gtest.h>

using namespace fabric::simulation;
using fabric::K_CHUNK_SIZE;

class ParallelSimulationTest : public ::testing::Test {
  protected:
    VoxelCell makeMaterial(MaterialId id) {
        VoxelCell c;
        c.materialId = id;
        return c;
    }

    void markAllSubRegions(ChunkActivityTracker& tracker, ChunkPos pos) {
        for (int lz = 0; lz < K_CHUNK_SIZE; lz += 8)
            for (int ly = 0; ly < K_CHUNK_SIZE; ly += 8)
                for (int lx = 0; lx < K_CHUNK_SIZE; lx += 8)
                    tracker.markSubRegionActive(pos, lx, ly, lz);
    }

    /// Set up a system with multiple chunks, each with sand at various heights.
    void setupMultiChunkWorld(VoxelSimulationSystem& sim, int numChunksX) {
        for (int cx = 0; cx < numChunksX; ++cx) {
            sim.grid().fillChunk(cx, 0, 0, VoxelCell{});
            sim.grid().materializeChunk(cx, 0, 0);
            sim.activityTracker().setState(ChunkPos{cx, 0, 0}, ChunkState::Active);
            markAllSubRegions(sim.activityTracker(), ChunkPos{cx, 0, 0});

            // Stone floor
            int baseX = cx * K_CHUNK_SIZE;
            for (int x = baseX; x < baseX + K_CHUNK_SIZE; ++x)
                for (int z = 0; z < K_CHUNK_SIZE; ++z)
                    sim.grid().writeCell(x, 0, z, makeMaterial(material_ids::STONE));

            // Sand at y=10
            sim.grid().writeCell(baseX + 16, 10, 16, makeMaterial(material_ids::SAND));
        }
        sim.grid().advanceEpoch();
    }

    /// Run N ticks, re-activating all chunks each tick.
    void runTicks(VoxelSimulationSystem& sim, int numChunksX, int ticks) {
        for (int t = 0; t < ticks; ++t) {
            for (int cx = 0; cx < numChunksX; ++cx) {
                sim.activityTracker().setState(ChunkPos{cx, 0, 0}, ChunkState::Active);
                markAllSubRegions(sim.activityTracker(), ChunkPos{cx, 0, 0});
            }
            sim.tick();
        }
    }

    /// Snapshot all voxels in a chunk for comparison.
    std::vector<VoxelCell> snapshotChunk(const SimulationGrid& grid, int cx, int cy, int cz) {
        std::vector<VoxelCell> result(K_CHUNK_SIZE * K_CHUNK_SIZE * K_CHUNK_SIZE);
        int baseX = cx * K_CHUNK_SIZE;
        int baseY = cy * K_CHUNK_SIZE;
        int baseZ = cz * K_CHUNK_SIZE;
        for (int lz = 0; lz < K_CHUNK_SIZE; ++lz)
            for (int ly = 0; ly < K_CHUNK_SIZE; ++ly)
                for (int lx = 0; lx < K_CHUNK_SIZE; ++lx) {
                    int idx = lx + ly * K_CHUNK_SIZE + lz * K_CHUNK_SIZE * K_CHUNK_SIZE;
                    result[idx] = grid.readCell(baseX + lx, baseY + ly, baseZ + lz);
                }
        return result;
    }
};

// 1. Single-thread and multi-thread produce identical results after 10 ticks
TEST_F(ParallelSimulationTest, IdenticalResults1vsN) {
    constexpr int K_CHUNKS = 4;
    constexpr int K_TICKS = 10;

    // Run with 1 thread (sequential)
    VoxelSimulationSystem seqSim;
    seqSim.workerPool().disableForTesting();
    setupMultiChunkWorld(seqSim, K_CHUNKS);
    runTicks(seqSim, K_CHUNKS, K_TICKS);

    // Run with multiple threads
    VoxelSimulationSystem parSim;
    setupMultiChunkWorld(parSim, K_CHUNKS);
    runTicks(parSim, K_CHUNKS, K_TICKS);

    // Compare all chunks
    for (int cx = 0; cx < K_CHUNKS; ++cx) {
        auto seqSnap = snapshotChunk(seqSim.grid(), cx, 0, 0);
        auto parSnap = snapshotChunk(parSim.grid(), cx, 0, 0);
        for (size_t i = 0; i < seqSnap.size(); ++i) {
            EXPECT_EQ(seqSnap[i].materialId, parSnap[i].materialId) << "Mismatch in chunk " << cx << " at index " << i;
        }
    }
}

// 2. No data race with multiple chunks and threads (TSan validates at runtime)
TEST_F(ParallelSimulationTest, NoDataRaceTSan) {
    constexpr int K_CHUNKS = 8;
    constexpr int K_TICKS = 5;

    VoxelSimulationSystem sim;
    setupMultiChunkWorld(sim, K_CHUNKS);

    // Run ticks -- TSan will flag any races
    EXPECT_NO_THROW(runTicks(sim, K_CHUNKS, K_TICKS));
}

// 3. Parallel is not slower than sequential for 8 chunks
TEST_F(ParallelSimulationTest, PerformanceScaling) {
    constexpr int K_CHUNKS = 8;
    constexpr int K_TICKS = 5;

    // Sequential timing
    VoxelSimulationSystem seqSim;
    seqSim.workerPool().disableForTesting();
    setupMultiChunkWorld(seqSim, K_CHUNKS);

    auto seqStart = std::chrono::high_resolution_clock::now();
    runTicks(seqSim, K_CHUNKS, K_TICKS);
    auto seqEnd = std::chrono::high_resolution_clock::now();
    auto seqUs = std::chrono::duration_cast<std::chrono::microseconds>(seqEnd - seqStart).count();

    // Parallel timing
    VoxelSimulationSystem parSim;
    setupMultiChunkWorld(parSim, K_CHUNKS);

    auto parStart = std::chrono::high_resolution_clock::now();
    runTicks(parSim, K_CHUNKS, K_TICKS);
    auto parEnd = std::chrono::high_resolution_clock::now();
    auto parUs = std::chrono::duration_cast<std::chrono::microseconds>(parEnd - parStart).count();

    // Parallel should not be dramatically slower (allow 3x margin for CI variance)
    EXPECT_LT(parUs, seqUs * 3) << "Parallel (" << parUs << "us) much slower than sequential (" << seqUs << "us)";
}

// 4. 100 chunks complete within 5 seconds (no deadlock)
TEST_F(ParallelSimulationTest, NoDeadlock100Chunks) {
    constexpr int K_CHUNKS = 100;
    VoxelSimulationSystem sim;

    for (int cx = 0; cx < K_CHUNKS; ++cx) {
        sim.grid().fillChunk(cx, 0, 0, VoxelCell{});
        sim.grid().materializeChunk(cx, 0, 0);
        sim.activityTracker().setState(ChunkPos{cx, 0, 0}, ChunkState::Active);
        markAllSubRegions(sim.activityTracker(), ChunkPos{cx, 0, 0});

        int baseX = cx * K_CHUNK_SIZE;
        for (int x = baseX; x < baseX + K_CHUNK_SIZE; ++x)
            for (int z = 0; z < K_CHUNK_SIZE; ++z)
                sim.grid().writeCell(x, 0, z, makeMaterial(material_ids::STONE));
        sim.grid().writeCell(baseX + 16, 10, 16, makeMaterial(material_ids::SAND));
    }
    sim.grid().advanceEpoch();

    auto start = std::chrono::steady_clock::now();
    for (int t = 0; t < 3; ++t) {
        for (int cx = 0; cx < K_CHUNKS; ++cx) {
            sim.activityTracker().setState(ChunkPos{cx, 0, 0}, ChunkState::Active);
            markAllSubRegions(sim.activityTracker(), ChunkPos{cx, 0, 0});
        }
        sim.tick();
    }
    auto end = std::chrono::steady_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    EXPECT_LT(ms, 5000) << "100 chunks took " << ms << "ms, expected < 5000ms";
}

// 5. Gravity tests still pass through the parallel dispatch path
TEST_F(ParallelSimulationTest, GravityTestsStillPass) {
    VoxelSimulationSystem sim;
    sim.grid().fillChunk(0, 0, 0, VoxelCell{});
    sim.grid().materializeChunk(0, 0, 0);

    // Stone floor
    for (int x = 0; x < K_CHUNK_SIZE; ++x)
        for (int z = 0; z < K_CHUNK_SIZE; ++z)
            sim.grid().writeCell(x, 0, z, makeMaterial(material_ids::STONE));
    // Contained column
    for (int y = 1; y <= 12; ++y) {
        sim.grid().writeCell(15, y, 16, makeMaterial(material_ids::STONE));
        sim.grid().writeCell(17, y, 16, makeMaterial(material_ids::STONE));
        sim.grid().writeCell(16, y, 15, makeMaterial(material_ids::STONE));
        sim.grid().writeCell(16, y, 17, makeMaterial(material_ids::STONE));
    }
    sim.grid().advanceEpoch();

    sim.grid().writeCell(16, 10, 16, makeMaterial(material_ids::SAND));
    sim.grid().advanceEpoch();

    for (int i = 0; i < 15; ++i) {
        sim.activityTracker().setState(ChunkPos{0, 0, 0}, ChunkState::Active);
        markAllSubRegions(sim.activityTracker(), ChunkPos{0, 0, 0});
        sim.tick();
    }

    EXPECT_EQ(sim.grid().readCell(16, 1, 16).materialId, material_ids::SAND);
}

// 6. Liquid tests still pass through the parallel dispatch path
TEST_F(ParallelSimulationTest, LiquidTestsStillPass) {
    VoxelSimulationSystem sim;
    sim.grid().fillChunk(0, 0, 0, VoxelCell{});
    sim.grid().materializeChunk(0, 0, 0);

    // Stone box
    for (int x = 10; x <= 14; ++x)
        for (int z = 10; z <= 14; ++z) {
            sim.grid().writeCell(x, 0, z, makeMaterial(material_ids::STONE));
            for (int y = 1; y <= 4; ++y)
                if (x == 10 || x == 14 || z == 10 || z == 14)
                    sim.grid().writeCell(x, y, z, makeMaterial(material_ids::STONE));
        }
    sim.grid().advanceEpoch();

    // Pour water
    for (int x = 11; x <= 13; ++x)
        for (int z = 11; z <= 13; ++z)
            sim.grid().writeCell(x, 4, z, makeMaterial(material_ids::WATER));
    sim.grid().advanceEpoch();

    for (int i = 0; i < 50; ++i) {
        sim.activityTracker().setState(ChunkPos{0, 0, 0}, ChunkState::Active);
        markAllSubRegions(sim.activityTracker(), ChunkPos{0, 0, 0});
        sim.tick();
    }

    // Water should settle at bottom
    int bottomWater = 0;
    for (int x = 11; x <= 13; ++x)
        for (int z = 11; z <= 13; ++z)
            if (sim.grid().readCell(x, 1, z).materialId == material_ids::WATER)
                ++bottomWater;
    EXPECT_EQ(bottomWater, 9);
}

// 7. disableForTesting() runs inline, deterministic
TEST_F(ParallelSimulationTest, DisableForTesting) {
    SimWorkerPool pool(4);
    pool.disableForTesting();
    EXPECT_EQ(pool.threadCount(), 0u);

    // Verify tasks run inline
    int counter = 0;
    std::vector<std::function<void(std::mt19937&)>> tasks;
    tasks.push_back([&counter](std::mt19937&) { ++counter; });
    tasks.push_back([&counter](std::mt19937&) { ++counter; });
    tasks.push_back([&counter](std::mt19937&) { ++counter; });

    pool.dispatchAndWait(tasks, 42);
    EXPECT_EQ(counter, 3);

    // Verify deterministic PRNG seeding: same seed produces same sequence
    std::vector<uint32_t> results1, results2;
    std::vector<std::function<void(std::mt19937&)>> seedTasks;
    seedTasks.push_back([&results1](std::mt19937& rng) { results1.push_back(rng()); });
    seedTasks.push_back([&results1](std::mt19937& rng) { results1.push_back(rng()); });

    pool.dispatchAndWait(seedTasks, 100);

    std::vector<std::function<void(std::mt19937&)>> seedTasks2;
    seedTasks2.push_back([&results2](std::mt19937& rng) { results2.push_back(rng()); });
    seedTasks2.push_back([&results2](std::mt19937& rng) { results2.push_back(rng()); });

    pool.dispatchAndWait(seedTasks2, 100);

    ASSERT_EQ(results1.size(), results2.size());
    for (size_t i = 0; i < results1.size(); ++i)
        EXPECT_EQ(results1[i], results2[i]);
}
