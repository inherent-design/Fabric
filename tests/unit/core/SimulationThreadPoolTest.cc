#include "fabric/core/SimulationThreadPool.hh"
#include <atomic>
#include <gtest/gtest.h>
#include <set>

using namespace fabric;

TEST(SimulationThreadPoolTest, ThreadCountAtLeastOne) {
    SimulationThreadPool pool(1);
    EXPECT_GE(pool.threadCount(), 1u);
}

TEST(SimulationThreadPoolTest, BarrierWithNoWorkReturnsImmediately) {
    SimulationThreadPool pool(1);
    pool.barrierSync(); // Should not block
}

TEST(SimulationThreadPoolTest, SingleChunkDispatch) {
    SimulationThreadPool pool(2);
    std::atomic<int> counter{0};
    std::vector<ChunkCoord> chunks = {{0, 0, 0}};

    pool.dispatchChunks(chunks, [&](const ChunkCoord&) { counter.fetch_add(1); });
    pool.barrierSync();

    EXPECT_EQ(counter.load(), 1);
}

TEST(SimulationThreadPoolTest, MultiChunkDispatch) {
    SimulationThreadPool pool(2);
    std::atomic<int> counter{0};
    std::vector<ChunkCoord> chunks;
    for (int i = 0; i < 10; ++i) {
        chunks.push_back({i, 0, 0});
    }

    pool.dispatchChunks(chunks, [&](const ChunkCoord&) { counter.fetch_add(1); });
    pool.barrierSync();

    EXPECT_EQ(counter.load(), 10);
}

TEST(SimulationThreadPoolTest, PauseForTestingRunsInline) {
    SimulationThreadPool pool(2);
    pool.pauseForTesting();

    std::atomic<int> counter{0};
    std::vector<ChunkCoord> chunks = {{0, 0, 0}, {1, 0, 0}};

    pool.dispatchChunks(chunks, [&](const ChunkCoord&) { counter.fetch_add(1); });
    // Tasks should already be complete (ran inline)
    EXPECT_EQ(counter.load(), 2);

    pool.barrierSync();
    pool.resumeAfterTesting();
}

TEST(SimulationThreadPoolTest, StressHundredChunks) {
    SimulationThreadPool pool(4);
    std::atomic<int> counter{0};
    std::vector<ChunkCoord> chunks;
    for (int i = 0; i < 100; ++i) {
        chunks.push_back({i % 10, i / 10, 0});
    }

    pool.dispatchChunks(chunks, [&](const ChunkCoord&) { counter.fetch_add(1); });
    pool.barrierSync();

    EXPECT_EQ(counter.load(), 100);
}

TEST(SimulationThreadPoolTest, ChunkCoordPassedCorrectly) {
    SimulationThreadPool pool(2);
    pool.pauseForTesting();

    std::vector<ChunkCoord> received;
    std::mutex mu;
    std::vector<ChunkCoord> chunks = {{1, 2, 3}, {4, 5, 6}};

    pool.dispatchChunks(chunks, [&](const ChunkCoord& c) {
        std::lock_guard<std::mutex> lock(mu);
        received.push_back(c);
    });
    pool.barrierSync();

    ASSERT_EQ(received.size(), 2u);
    // Sort for deterministic comparison
    std::sort(received.begin(), received.end(), [](const ChunkCoord& a, const ChunkCoord& b) { return a.x < b.x; });
    EXPECT_EQ(received[0].x, 1);
    EXPECT_EQ(received[0].y, 2);
    EXPECT_EQ(received[0].z, 3);
    EXPECT_EQ(received[1].x, 4);
    EXPECT_EQ(received[1].y, 5);
    EXPECT_EQ(received[1].z, 6);

    pool.resumeAfterTesting();
}
