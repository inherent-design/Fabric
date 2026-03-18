#include "recurse/world/ChunkStreaming.hh"
#include <gtest/gtest.h>
#include <set>

using namespace recurse;

class ChunkStreamingTest : public ::testing::Test {
  protected:
    StreamingConfig smallConfig() { return {.baseRadius = 2, .maxLoadsPerTick = 1000, .maxUnloadsPerTick = 1000}; }

    int cubeChunkCount(int radius) {
        int side = 2 * radius + 1;
        return side * side * side;
    }

    bool inDesiredCube(const ChunkCoord& c, int centerCX, int centerCY, int centerCZ, int radius) {
        return std::abs(c.x - centerCX) <= radius && std::abs(c.y - centerCY) <= radius &&
               std::abs(c.z - centerCZ) <= radius;
    }
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

TEST_F(ChunkStreamingTest, StationaryRadiusEightDoesNotChurnWhenDesiredExceedsTrackedCap) {
    StreamingConfig cfg = {
        .baseRadius = 8,
        .maxLoadsPerTick = 4,
        .maxUnloadsPerTick = 4,
        .maxTrackedChunks = 4096,
    };
    ChunkStreamingManager mgr(cfg);

    int desiredCount = cubeChunkCount(cfg.baseRadius);
    int maxTicks = (desiredCount + cfg.maxLoadsPerTick - 1) / cfg.maxLoadsPerTick;
    for (int tick = 0; tick < maxTicks; ++tick) {
        auto result = mgr.update(0.0f, 0.0f, 0.0f);
        EXPECT_TRUE(result.toUnload.empty()) << "Stationary update should not churn at tick " << tick;
        EXPECT_LE(static_cast<int>(result.toLoad.size()), cfg.maxLoadsPerTick);
    }

    EXPECT_EQ(static_cast<int>(mgr.trackedChunkCount()), desiredCount);

    auto settled = mgr.update(0.0f, 0.0f, 0.0f);
    EXPECT_TRUE(settled.toLoad.empty());
    EXPECT_TRUE(settled.toUnload.empty());
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

TEST_F(ChunkStreamingTest, MaxTrackedChunksDoesNotEvictDesiredChunks) {
    StreamingConfig cfg = smallConfig();
    cfg.baseRadius = 2;
    cfg.maxLoadsPerTick = 10000;
    cfg.maxUnloadsPerTick = 10000;
    cfg.maxTrackedChunks = 10;
    ChunkStreamingManager mgr(cfg);

    auto result = mgr.update(0.0f, 0.0f, 0.0f);

    EXPECT_EQ(static_cast<int>(mgr.trackedChunkCount()), cubeChunkCount(cfg.baseRadius));
    EXPECT_TRUE(result.toUnload.empty());
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

TEST_F(ChunkStreamingTest, MaxTrackedChunksEvictsOnlyNonDesiredChunks) {
    StreamingConfig cfg = smallConfig();
    cfg.baseRadius = 2;
    cfg.maxLoadsPerTick = 10000;
    cfg.maxUnloadsPerTick = 5;
    cfg.maxTrackedChunks = 110;
    ChunkStreamingManager mgr(cfg);

    mgr.update(0.0f, 0.0f, 0.0f);
    auto result = mgr.update(32.0f, 0.0f, 0.0f);

    EXPECT_EQ(static_cast<int>(result.toLoad.size()), 25);
    EXPECT_EQ(static_cast<int>(result.toUnload.size()), 25);
    EXPECT_EQ(static_cast<int>(mgr.trackedChunkCount()), cubeChunkCount(cfg.baseRadius));
    EXPECT_GT(static_cast<int>(mgr.trackedChunkCount()), cfg.maxTrackedChunks);

    for (const auto& c : result.toUnload) {
        EXPECT_FALSE(inDesiredCube(c, 1, 0, 0, cfg.baseRadius));
    }
}
