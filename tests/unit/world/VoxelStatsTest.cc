#include <gtest/gtest.h>

#include "recurse/world/VoxelStats.hh"
#include <climits>

namespace recurse {

TEST(VoxelStatsTest, DefaultValues) {
    VoxelStats stats;
    EXPECT_EQ(stats.visibleChunks, 0);
    EXPECT_EQ(stats.totalChunks, 0);
    EXPECT_EQ(stats.meshQueueSize, 0);
}

TEST(VoxelStatsTest, SetAndRead) {
    VoxelStats stats;
    stats.visibleChunks = 50;
    stats.totalChunks = 200;
    stats.meshQueueSize = 12;

    EXPECT_EQ(stats.visibleChunks, 50);
    EXPECT_EQ(stats.totalChunks, 200);
    EXPECT_EQ(stats.meshQueueSize, 12);
}

TEST(VoxelStatsTest, LargeValues) {
    VoxelStats stats;
    stats.totalChunks = INT32_MAX;
    stats.meshQueueSize = INT32_MAX;

    EXPECT_EQ(stats.totalChunks, INT32_MAX);
    EXPECT_EQ(stats.meshQueueSize, INT32_MAX);
}

} // namespace recurse
