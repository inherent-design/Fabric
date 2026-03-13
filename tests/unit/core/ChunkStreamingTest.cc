#include "recurse/world/ChunkStreaming.hh"
#include <gtest/gtest.h>
#include <set>

using namespace recurse;

class ChunkStreamingTest : public ::testing::Test {
  protected:
    StreamingConfig smallConfig() { return {.baseRadius = 2, .maxLoadsPerTick = 1000, .maxUnloadsPerTick = 1000}; }
};

TEST_F(ChunkStreamingTest, InitialUpdateLoadsChunksAroundOrigin) {
    ChunkStreamingManager mgr(smallConfig());
    auto result = mgr.update(0.0f, 0.0f, 0.0f);
    EXPECT_FALSE(result.toLoad.empty());

    int side = 2 * 2 + 1; // baseRadius=2 -> 5x5x5
    EXPECT_EQ(static_cast<int>(result.toLoad.size()), side * side * side);
}

TEST_F(ChunkStreamingTest, RadiusIsConstant) {
    StreamingConfig cfg = smallConfig();
    cfg.maxLoadsPerTick = 10000;
    ChunkStreamingManager mgr(cfg);

    mgr.update(0.0f, 0.0f, 0.0f);
    EXPECT_EQ(mgr.currentRadius(), cfg.baseRadius);

    // Moving to a new position does not change the radius
    mgr.update(1000.0f, 0.0f, 0.0f);
    EXPECT_EQ(mgr.currentRadius(), cfg.baseRadius);
}

TEST_F(ChunkStreamingTest, UnloadsFarChunks) {
    StreamingConfig cfg = smallConfig();
    cfg.maxLoadsPerTick = 10000;
    cfg.maxUnloadsPerTick = 10000;
    ChunkStreamingManager mgr(cfg);

    mgr.update(0.0f, 0.0f, 0.0f);
    auto result = mgr.update(10000.0f, 0.0f, 0.0f);
    EXPECT_FALSE(result.toUnload.empty());
}

TEST_F(ChunkStreamingTest, BudgetRespected) {
    StreamingConfig cfg = smallConfig();
    cfg.maxLoadsPerTick = 3;
    ChunkStreamingManager mgr(cfg);
    auto result = mgr.update(0.0f, 0.0f, 0.0f);
    EXPECT_LE(static_cast<int>(result.toLoad.size()), 3);
}

TEST_F(ChunkStreamingTest, PrioritizesNearestChunks) {
    StreamingConfig cfg = smallConfig();
    cfg.maxLoadsPerTick = 5;
    ChunkStreamingManager mgr(cfg);
    auto result = mgr.update(16.0f, 16.0f, 16.0f);

    if (result.toLoad.size() >= 2) {
        int centerCX = 0, centerCY = 0, centerCZ = 0;
        auto distSq = [&](const ChunkCoord& c) {
            int dx = c.x - centerCX;
            int dy = c.y - centerCY;
            int dz = c.z - centerCZ;
            return dx * dx + dy * dy + dz * dz;
        };
        for (size_t i = 1; i < result.toLoad.size(); ++i) {
            EXPECT_LE(distSq(result.toLoad[i - 1]), distSq(result.toLoad[i]));
        }
    }
}

TEST_F(ChunkStreamingTest, StationaryNoUpdates) {
    StreamingConfig cfg = smallConfig();
    cfg.maxLoadsPerTick = 10000;
    cfg.maxUnloadsPerTick = 10000;
    ChunkStreamingManager mgr(cfg);

    mgr.update(0.0f, 0.0f, 0.0f);
    auto result = mgr.update(0.0f, 0.0f, 0.0f);
    EXPECT_TRUE(result.toLoad.empty());
    EXPECT_TRUE(result.toUnload.empty());
}

TEST_F(ChunkStreamingTest, NegativeCoordinates) {
    StreamingConfig cfg = smallConfig();
    cfg.maxLoadsPerTick = 10000;
    ChunkStreamingManager mgr(cfg);
    auto result = mgr.update(-100.0f, -50.0f, -200.0f);
    EXPECT_FALSE(result.toLoad.empty());

    for (const auto& c : result.toLoad) {
        EXPECT_NE(c.x, 0);
    }
}

TEST_F(ChunkStreamingTest, MaxTrackedChunksEvictsFarthest) {
    StreamingConfig cfg = smallConfig();
    cfg.baseRadius = 2;
    cfg.maxLoadsPerTick = 10000;
    cfg.maxUnloadsPerTick = 10000;
    cfg.maxTrackedChunks = 10;
    ChunkStreamingManager mgr(cfg);

    // Load all chunks at origin (5^3 = 125 desired, but cap at 10)
    auto result = mgr.update(0.0f, 0.0f, 0.0f);

    // After the first update, tracked count should not exceed the cap
    EXPECT_LE(static_cast<int>(mgr.trackedChunkCount()), cfg.maxTrackedChunks);

    // The excess chunks should appear in toUnload
    // 125 loaded initially, then evicted down to 10
    int totalLoaded = static_cast<int>(result.toLoad.size());
    int totalUnloaded = static_cast<int>(result.toUnload.size());
    EXPECT_EQ(static_cast<int>(mgr.trackedChunkCount()), totalLoaded - totalUnloaded);
}

TEST_F(ChunkStreamingTest, MaxTrackedChunksZeroMeansUnlimited) {
    StreamingConfig cfg = smallConfig();
    cfg.baseRadius = 2;
    cfg.maxLoadsPerTick = 10000;
    cfg.maxTrackedChunks = 0; // unlimited
    ChunkStreamingManager mgr(cfg);

    auto result = mgr.update(0.0f, 0.0f, 0.0f);

    int side = 2 * 2 + 1;
    EXPECT_EQ(static_cast<int>(mgr.trackedChunkCount()), side * side * side);
    EXPECT_TRUE(result.toUnload.empty());
}

TEST_F(ChunkStreamingTest, MaxTrackedChunksKeepsNearestChunks) {
    StreamingConfig cfg = smallConfig();
    cfg.baseRadius = 2;
    cfg.maxLoadsPerTick = 10000;
    cfg.maxUnloadsPerTick = 10000;
    cfg.maxTrackedChunks = 27; // 3^3
    ChunkStreamingManager mgr(cfg);

    auto result = mgr.update(0.0f, 0.0f, 0.0f);

    // The chunks that survived eviction should all be within radius 1 of center
    // (3^3 = 27 nearest chunks)
    EXPECT_EQ(static_cast<int>(mgr.trackedChunkCount()), 27);

    // Evicted chunks should be farther than kept chunks
    // Verify no evicted chunk is closer than any kept chunk
    if (!result.toUnload.empty()) {
        auto distSq = [](const ChunkCoord& c) {
            return c.x * c.x + c.y * c.y + c.z * c.z;
        };
        int minUnloadDist = distSq(result.toUnload.back()); // farthest-first sorted
        for (const auto& loaded : result.toLoad) {
            bool wasUnloaded = false;
            for (const auto& u : result.toUnload) {
                if (u == loaded) {
                    wasUnloaded = true;
                    break;
                }
            }
            if (!wasUnloaded) {
                EXPECT_LE(distSq(loaded), minUnloadDist);
            }
        }
    }
}
