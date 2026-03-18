#include "recurse/world/ChunkStreaming.hh"

#include <gtest/gtest.h>
#include <set>

using namespace recurse;

class StreamingMultiPositionTest : public ::testing::Test {
  protected:
    StreamingConfig bigBudgetConfig() {
        return {.baseRadius = 2, .maxLoadsPerTick = 10000, .maxUnloadsPerTick = 10000};
    }
};

TEST_F(StreamingMultiPositionTest, SingleSourceCompat) {
    auto cfg = bigBudgetConfig();
    ChunkStreamingManager mgr(cfg);

    auto singlePos = mgr.update(0.0f, 0.0f, 0.0f);

    ChunkStreamingManager mgr2(cfg);
    auto multiPos = mgr2.update({{0.0f, 0.0f, 0.0f, cfg.baseRadius}});

    EXPECT_EQ(singlePos.toLoad.size(), multiPos.toLoad.size());

    std::set<std::tuple<int, int, int>> singleSet, multiSet;
    for (const auto& c : singlePos.toLoad)
        singleSet.insert({c.x, c.y, c.z});
    for (const auto& c : multiPos.toLoad)
        multiSet.insert({c.x, c.y, c.z});
    EXPECT_EQ(singleSet, multiSet);
}

TEST_F(StreamingMultiPositionTest, TwoSourcesUnion) {
    auto cfg = bigBudgetConfig();
    cfg.baseRadius = 1;
    ChunkStreamingManager mgr(cfg);

    // Two sources far apart so their cubes don't overlap
    auto result = mgr.update({
        {0.0f, 0.0f, 0.0f, 1},
        {10.0f * 32.0f, 0.0f, 0.0f, 1},
    });

    // radius=1 -> 3x3x3=27 per source. No overlap -> 54 total.
    EXPECT_EQ(result.toLoad.size(), 54u);
}

TEST_F(StreamingMultiPositionTest, OverlapDedup) {
    auto cfg = bigBudgetConfig();
    ChunkStreamingManager mgr(cfg);

    // Two sources at same position; should produce same chunks as one source
    auto result = mgr.update({
        {16.0f, 16.0f, 16.0f, 2},
        {16.0f, 16.0f, 16.0f, 2},
    });

    int side = 2 * 2 + 1;
    EXPECT_EQ(static_cast<int>(result.toLoad.size()), side * side * side);
}

TEST_F(StreamingMultiPositionTest, UnloadRequiresAllSourcesBeyond) {
    auto cfg = bigBudgetConfig();
    cfg.baseRadius = 1;
    ChunkStreamingManager mgr(cfg);

    // Load chunks around two sources
    mgr.update({
        {0.0f, 0.0f, 0.0f, 1},
        {2.0f * 32.0f, 0.0f, 0.0f, 1},
    });

    // Remove source B but keep source A. Source A's radius=1 covers chunks -1..1.
    // Chunk (2,0,0) was within B's radius but outside A's; should unload.
    auto result = mgr.update({{0.0f, 0.0f, 0.0f, 1}});

    bool unloaded2 = false;
    for (const auto& c : result.toUnload) {
        if (c.x == 2 && c.y == 0 && c.z == 0)
            unloaded2 = true;
    }
    // Chunk (1,0,0) is within both sources; should NOT appear in unload
    bool unloaded1 = false;
    for (const auto& c : result.toUnload) {
        if (c.x == 1 && c.y == 0 && c.z == 0)
            unloaded1 = true;
    }

    EXPECT_TRUE(unloaded2) << "Chunk exclusive to removed source should unload";
    EXPECT_FALSE(unloaded1) << "Chunk within remaining source should stay";
}

TEST_F(StreamingMultiPositionTest, MinDistSortMultiSource) {
    StreamingConfig cfg = {.baseRadius = 2, .maxLoadsPerTick = 5, .maxUnloadsPerTick = 10000};
    ChunkStreamingManager mgr(cfg);

    auto result = mgr.update({
        {0.0f, 0.0f, 0.0f, 2},
        {5.0f * 32.0f, 0.0f, 0.0f, 2},
    });

    // With budget=5, first 5 loaded chunks should be the nearest to either source
    if (result.toLoad.size() >= 2) {
        auto minDistSq = [](const ChunkCoord& c, int sx1, int sx2) {
            int d1 = c.x - sx1;
            int d1sq = d1 * d1 + c.y * c.y + c.z * c.z;
            int d2 = c.x - sx2;
            int d2sq = d2 * d2 + c.y * c.y + c.z * c.z;
            return std::min(d1sq, d2sq);
        };
        for (size_t i = 1; i < result.toLoad.size(); ++i) {
            EXPECT_LE(minDistSq(result.toLoad[i - 1], 0, 5), minDistSq(result.toLoad[i], 0, 5))
                << "Load order should be sorted by min distance to any source";
        }
    }
}

TEST_F(StreamingMultiPositionTest, SourceRemovalTriggersUnload) {
    auto cfg = bigBudgetConfig();
    cfg.baseRadius = 1;
    ChunkStreamingManager mgr(cfg);

    // Load with two far-apart sources
    mgr.update({
        {0.0f, 0.0f, 0.0f, 1},
        {20.0f * 32.0f, 0.0f, 0.0f, 1},
    });

    // Remove second source
    auto result = mgr.update({{0.0f, 0.0f, 0.0f, 1}});

    // Chunks around (20,0,0) should appear in unload
    int unloadedFromB = 0;
    for (const auto& c : result.toUnload) {
        if (std::abs(c.x - 20) <= 1 && std::abs(c.y) <= 1 && std::abs(c.z) <= 1)
            ++unloadedFromB;
    }
    EXPECT_EQ(unloadedFromB, 27) << "All 3x3x3 chunks exclusive to removed source should unload";
}

TEST_F(StreamingMultiPositionTest, BudgetEvictionMultiSourceOnlyRemovesChunksOutsideDesiredUnion) {
    StreamingConfig cfg = {
        .baseRadius = 2,
        .maxLoadsPerTick = 10000,
        .maxUnloadsPerTick = 5,
        .maxTrackedChunks = 100,
    };
    ChunkStreamingManager mgr(cfg);

    mgr.update({
        {0.0f, 0.0f, 0.0f, 2},
        {20.0f * 32.0f, 0.0f, 0.0f, 2},
    });

    auto result = mgr.update({
        {0.0f, 0.0f, 0.0f, 2},
    });

    EXPECT_EQ(static_cast<int>(result.toUnload.size()), 125);
    EXPECT_EQ(static_cast<int>(mgr.trackedChunkCount()), 125);
    EXPECT_GT(static_cast<int>(mgr.trackedChunkCount()), cfg.maxTrackedChunks);

    for (const auto& c : result.toUnload) {
        EXPECT_FALSE(std::abs(c.x) <= 2 && std::abs(c.y) <= 2 && std::abs(c.z) <= 2)
            << "Unloaded chunk at (" << c.x << "," << c.y << "," << c.z
            << ") should be outside the remaining desired union";
    }
}
