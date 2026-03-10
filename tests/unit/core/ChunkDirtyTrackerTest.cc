#include "fabric/world/ChunkDirtyTracker.hh"
#include <gtest/gtest.h>

using namespace fabric;

class ChunkDirtyTrackerTest : public ::testing::Test {
  protected:
    ChunkDirtyTracker tracker;
};

TEST_F(ChunkDirtyTrackerTest, DefaultStateIsSleeping) {
    EXPECT_EQ(tracker.getState({0, 0, 0}), ChunkState::Sleeping);
    EXPECT_EQ(tracker.getState({99, -5, 42}), ChunkState::Sleeping);
}

TEST_F(ChunkDirtyTrackerTest, MarkActiveReturnsActive) {
    tracker.markActive({1, 2, 3});
    EXPECT_EQ(tracker.getState({1, 2, 3}), ChunkState::Active);
}

TEST_F(ChunkDirtyTrackerTest, MarkSleepingReturnsSleeping) {
    tracker.markActive({1, 0, 0});
    tracker.markSleeping({1, 0, 0});
    EXPECT_EQ(tracker.getState({1, 0, 0}), ChunkState::Sleeping);
}

TEST_F(ChunkDirtyTrackerTest, MarkBoundaryDirtyOnSleeping) {
    tracker.markBoundaryDirty({2, 0, 0});
    EXPECT_EQ(tracker.getState({2, 0, 0}), ChunkState::BoundaryDirty);
}

TEST_F(ChunkDirtyTrackerTest, MarkBoundaryDirtyOnActiveStaysActive) {
    tracker.markActive({3, 0, 0});
    tracker.markBoundaryDirty({3, 0, 0});
    EXPECT_EQ(tracker.getState({3, 0, 0}), ChunkState::Active);
}

TEST_F(ChunkDirtyTrackerTest, CollectActiveChunksIncludesNonSleeping) {
    tracker.markActive({0, 0, 0});
    tracker.markBoundaryDirty({1, 0, 0});
    // Sleeping chunk should not appear
    tracker.markActive({2, 0, 0});
    tracker.markSleeping({2, 0, 0});

    auto active = tracker.collectActiveChunks();
    EXPECT_EQ(active.size(), 2u);
}

TEST_F(ChunkDirtyTrackerTest, WakeNeighborsMarks6) {
    tracker.markActive({5, 5, 5});
    tracker.wakeNeighbors({5, 5, 5});

    EXPECT_EQ(tracker.getState({6, 5, 5}), ChunkState::BoundaryDirty);
    EXPECT_EQ(tracker.getState({4, 5, 5}), ChunkState::BoundaryDirty);
    EXPECT_EQ(tracker.getState({5, 6, 5}), ChunkState::BoundaryDirty);
    EXPECT_EQ(tracker.getState({5, 4, 5}), ChunkState::BoundaryDirty);
    EXPECT_EQ(tracker.getState({5, 5, 6}), ChunkState::BoundaryDirty);
    EXPECT_EQ(tracker.getState({5, 5, 4}), ChunkState::BoundaryDirty);
}

TEST_F(ChunkDirtyTrackerTest, SubChunkBitmask) {
    ChunkCoord pos{0, 0, 0};
    tracker.markActive(pos);

    // Bit 0 at (0,0,0)
    tracker.setSubChunkDirty(pos, 0, 0, 0);
    EXPECT_EQ(tracker.getSubChunkMask(pos) & 1u, 1u);

    // Bit 63 at (3,3,3): 3*16 + 3*4 + 3 = 48 + 12 + 3 = 63
    tracker.setSubChunkDirty(pos, 3, 3, 3);
    EXPECT_NE(tracker.getSubChunkMask(pos) & (uint64_t{1} << 63), 0u);
}

TEST_F(ChunkDirtyTrackerTest, ClearSubChunkMask) {
    ChunkCoord pos{0, 0, 0};
    tracker.markActive(pos);
    tracker.setSubChunkDirty(pos, 1, 1, 1);
    EXPECT_NE(tracker.getSubChunkMask(pos), 0u);

    tracker.clearSubChunkMask(pos);
    EXPECT_EQ(tracker.getSubChunkMask(pos), 0u);
}

TEST_F(ChunkDirtyTrackerTest, PriorityOrdering) {
    tracker.setReferencePoint(0, 0, 0);

    // Immediate: distance < 3
    tracker.markActive({1, 0, 0});
    // Normal: distance < 7
    tracker.markActive({5, 0, 0});
    // Background: distance < 12
    tracker.markActive({10, 0, 0});
    // Hibernating: distance >= 12
    tracker.markActive({20, 0, 0});

    auto active = tracker.collectActiveChunks();
    ASSERT_EQ(active.size(), 4u);

    // Verify ordering: Immediate first, then Normal, Background, Hibernating
    EXPECT_EQ(active[0].x, 1);  // Immediate
    EXPECT_EQ(active[1].x, 5);  // Normal
    EXPECT_EQ(active[2].x, 10); // Background
    EXPECT_EQ(active[3].x, 20); // Hibernating
}

TEST_F(ChunkDirtyTrackerTest, MaxCountCapsResults) {
    tracker.setReferencePoint(0, 0, 0);
    for (int i = 0; i < 10; ++i) {
        tracker.markActive({i, 0, 0});
    }

    auto capped = tracker.collectActiveChunks(3);
    EXPECT_EQ(capped.size(), 3u);
}

TEST_F(ChunkDirtyTrackerTest, ActiveCount) {
    EXPECT_EQ(tracker.activeCount(), 0u);

    tracker.markActive({0, 0, 0});
    tracker.markActive({1, 0, 0});
    tracker.markBoundaryDirty({2, 0, 0});
    EXPECT_EQ(tracker.activeCount(), 3u);

    tracker.markSleeping({0, 0, 0});
    EXPECT_EQ(tracker.activeCount(), 2u);
}

TEST_F(ChunkDirtyTrackerTest, TotalTracked) {
    tracker.markActive({0, 0, 0});
    tracker.markActive({1, 0, 0});
    tracker.markSleeping({1, 0, 0}); // Still tracked, just sleeping
    EXPECT_EQ(tracker.totalTracked(), 2u);
}

TEST_F(ChunkDirtyTrackerTest, MarkSleepingClearsSubChunkMask) {
    ChunkCoord pos{0, 0, 0};
    tracker.markActive(pos);
    tracker.setSubChunkDirty(pos, 2, 2, 2);
    EXPECT_NE(tracker.getSubChunkMask(pos), 0u);

    tracker.markSleeping(pos);
    EXPECT_EQ(tracker.getSubChunkMask(pos), 0u);
}
