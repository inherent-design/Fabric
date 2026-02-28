#include "fabric/core/ChunkStreaming.hh"
#include <gtest/gtest.h>
#include <set>

using namespace fabric;

class ChunkStreamingTest : public ::testing::Test {
  protected:
    StreamingConfig smallConfig() {
        return {
            .baseRadius = 2, .maxRadius = 4, .speedScale = 0.5f, .maxLoadsPerTick = 1000, .maxUnloadsPerTick = 1000};
    }
};

TEST_F(ChunkStreamingTest, InitialUpdateLoadsChunksAroundOrigin) {
    ChunkStreamingManager mgr(smallConfig());
    auto result = mgr.update(0.0f, 0.0f, 0.0f, 0.0f);
    EXPECT_FALSE(result.toLoad.empty());

    int side = 2 * 2 + 1; // baseRadius=2 -> 5x5x5
    EXPECT_EQ(static_cast<int>(result.toLoad.size()), side * side * side);
}

TEST_F(ChunkStreamingTest, MovingIncreasesRadius) {
    StreamingConfig cfg = smallConfig();
    cfg.maxLoadsPerTick = 10000;
    ChunkStreamingManager mgrSlow(cfg);
    ChunkStreamingManager mgrFast(cfg);

    auto slow = mgrSlow.update(0.0f, 0.0f, 0.0f, 0.0f);
    auto fast = mgrFast.update(0.0f, 0.0f, 0.0f, 4.0f);

    EXPECT_GT(mgrFast.currentRadius(), mgrSlow.currentRadius());
    EXPECT_GT(fast.toLoad.size(), slow.toLoad.size());
}

TEST_F(ChunkStreamingTest, MaxRadiusClamped) {
    StreamingConfig cfg = smallConfig();
    cfg.maxLoadsPerTick = 100000;
    ChunkStreamingManager mgr(cfg);
    mgr.update(0.0f, 0.0f, 0.0f, 1000.0f);
    EXPECT_EQ(mgr.currentRadius(), cfg.maxRadius);
}

TEST_F(ChunkStreamingTest, UnloadsFarChunks) {
    StreamingConfig cfg = smallConfig();
    cfg.maxLoadsPerTick = 10000;
    cfg.maxUnloadsPerTick = 10000;
    ChunkStreamingManager mgr(cfg);

    mgr.update(0.0f, 0.0f, 0.0f, 0.0f);
    auto result = mgr.update(10000.0f, 0.0f, 0.0f, 0.0f);
    EXPECT_FALSE(result.toUnload.empty());
}

TEST_F(ChunkStreamingTest, BudgetRespected) {
    StreamingConfig cfg = smallConfig();
    cfg.maxLoadsPerTick = 3;
    ChunkStreamingManager mgr(cfg);
    auto result = mgr.update(0.0f, 0.0f, 0.0f, 0.0f);
    EXPECT_LE(static_cast<int>(result.toLoad.size()), 3);
}

TEST_F(ChunkStreamingTest, PrioritizesNearestChunks) {
    StreamingConfig cfg = smallConfig();
    cfg.maxLoadsPerTick = 5;
    ChunkStreamingManager mgr(cfg);
    auto result = mgr.update(16.0f, 16.0f, 16.0f, 0.0f);

    if (result.toLoad.size() >= 2) {
        int centerCX = 0, centerCY = 0, centerCZ = 0;
        auto distSq = [&](const ChunkCoord& c) {
            int dx = c.cx - centerCX;
            int dy = c.cy - centerCY;
            int dz = c.cz - centerCZ;
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

    mgr.update(0.0f, 0.0f, 0.0f, 0.0f);
    auto result = mgr.update(0.0f, 0.0f, 0.0f, 0.0f);
    EXPECT_TRUE(result.toLoad.empty());
    EXPECT_TRUE(result.toUnload.empty());
}

TEST_F(ChunkStreamingTest, NegativeCoordinates) {
    StreamingConfig cfg = smallConfig();
    cfg.maxLoadsPerTick = 10000;
    ChunkStreamingManager mgr(cfg);
    auto result = mgr.update(-100.0f, -50.0f, -200.0f, 0.0f);
    EXPECT_FALSE(result.toLoad.empty());

    for (const auto& c : result.toLoad) {
        EXPECT_NE(c.cx, 0);
    }
}
