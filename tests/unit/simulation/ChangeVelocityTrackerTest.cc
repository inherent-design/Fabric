#include "recurse/simulation/ChangeVelocityTracker.hh"
#include <gtest/gtest.h>

using namespace recurse::simulation;

class ChangeVelocityTrackerTest : public ::testing::Test {
  protected:
    static ChangeVelocityTracker makeTracker(uint32_t ringSize = 8, uint64_t windowFrames = 240) {
        ChangeVelocityConfig cfg;
        cfg.ringSize = ringSize;
        cfg.windowFrames = windowFrames;
        return ChangeVelocityTracker(cfg);
    }
};

// 1. Push N entries, verify size and at() ordering
TEST_F(ChangeVelocityTrackerTest, RingPushAndSize) {
    ChunkVelocityRing ring(4);
    EXPECT_EQ(ring.size(), 0u);
    EXPECT_EQ(ring.capacity(), 4u);

    ring.push(10, 5);
    ring.push(20, 10);
    ring.push(30, 15);
    EXPECT_EQ(ring.size(), 3u);

    EXPECT_EQ(ring.at(0).frame, 10u);
    EXPECT_EQ(ring.at(0).swapCount, 5u);
    EXPECT_EQ(ring.at(1).frame, 20u);
    EXPECT_EQ(ring.at(2).frame, 30u);
    EXPECT_EQ(ring.at(2).swapCount, 15u);
}

// 2. Push > capacity entries, verify oldest evicted
TEST_F(ChangeVelocityTrackerTest, RingOverwrite) {
    ChunkVelocityRing ring(3);

    ring.push(1, 10);
    ring.push(2, 20);
    ring.push(3, 30);
    EXPECT_EQ(ring.size(), 3u);

    ring.push(4, 40);
    EXPECT_EQ(ring.size(), 3u);

    // Oldest (frame=1) should be evicted
    EXPECT_EQ(ring.at(0).frame, 2u);
    EXPECT_EQ(ring.at(1).frame, 3u);
    EXPECT_EQ(ring.at(2).frame, 4u);
    EXPECT_EQ(ring.at(2).swapCount, 40u);
}

// 3. segments() returns single span when not full
TEST_F(ChangeVelocityTrackerTest, RingSegmentsNoWrap) {
    ChunkVelocityRing ring(8);
    ring.push(1, 10);
    ring.push(2, 20);
    ring.push(3, 30);

    auto [seg1, seg2] = ring.segments();
    EXPECT_EQ(seg1.size(), 3u);
    EXPECT_TRUE(seg2.empty());
    EXPECT_EQ(seg1[0].frame, 1u);
    EXPECT_EQ(seg1[2].frame, 3u);
}

// 4. segments() returns two spans when wrapped
TEST_F(ChangeVelocityTrackerTest, RingSegmentsWrap) {
    ChunkVelocityRing ring(3);
    ring.push(1, 10);
    ring.push(2, 20);
    ring.push(3, 30);
    ring.push(4, 40); // overwrites slot 0, head moves to 1

    auto [seg1, seg2] = ring.segments();
    // Total entries across both segments = capacity
    EXPECT_EQ(seg1.size() + seg2.size(), 3u);

    // Verify all entries are accessible via at() in logical order
    EXPECT_EQ(ring.at(0).frame, 2u);
    EXPECT_EQ(ring.at(1).frame, 3u);
    EXPECT_EQ(ring.at(2).frame, 4u);
}

// 5. record() for unknown chunk creates ring
TEST_F(ChangeVelocityTrackerTest, RecordCreatesRingLazily) {
    auto tracker = makeTracker();
    fabric::ChunkCoord pos{5, 10, 15};

    EXPECT_EQ(tracker.history(pos), nullptr);
    tracker.record(pos, 42, 100);
    EXPECT_NE(tracker.history(pos), nullptr);
    EXPECT_EQ(tracker.history(pos)->size(), 1u);
    EXPECT_EQ(tracker.history(pos)->at(0).swapCount, 42u);
}

// 6. velocity() returns 0 for untracked chunk
TEST_F(ChangeVelocityTrackerTest, VelocityZeroForUntracked) {
    auto tracker = makeTracker();
    EXPECT_FLOAT_EQ(tracker.velocity({99, 99, 99}), 0.0f);
}

// 7. velocity() with one entry returns raw swapCount as float
TEST_F(ChangeVelocityTrackerTest, VelocitySingleEntry) {
    auto tracker = makeTracker();
    fabric::ChunkCoord pos{0, 0, 0};
    tracker.record(pos, 100, 60);
    EXPECT_FLOAT_EQ(tracker.velocity(pos), 100.0f);
}

// 8. Record entries spanning the window, verify swaps/sec
TEST_F(ChangeVelocityTrackerTest, VelocityOverWindow) {
    // windowFrames=120 -> 2 seconds at 60 TPS
    auto tracker = makeTracker(8, 120);
    fabric::ChunkCoord pos{0, 0, 0};

    // Record at frames 0, 60, 120: all within 120 frames of newest (frame 120)
    tracker.record(pos, 10, 0);
    tracker.record(pos, 20, 60);
    tracker.record(pos, 30, 120);

    // totalSwaps = 10+20+30 = 60
    // dt = (120 - 0) / 60.0 = 2.0 seconds
    // velocity = 60 / 2.0 = 30.0 swaps/sec
    EXPECT_FLOAT_EQ(tracker.velocity(pos), 30.0f);
}

// 9. Entries older than windowFrames excluded from sum
TEST_F(ChangeVelocityTrackerTest, VelocityIgnoresOldEntries) {
    // windowFrames=60 -> 1 second at 60 TPS
    auto tracker = makeTracker(8, 60);
    fabric::ChunkCoord pos{0, 0, 0};

    // frame 0: outside the window when newest is frame 120
    tracker.record(pos, 100, 0);
    // frame 90: within 60 frames of 120
    tracker.record(pos, 10, 90);
    // frame 120: newest
    tracker.record(pos, 20, 120);

    // Only frames 90 and 120 are within window (120 - 60 = 60, frame 0 < 60)
    // totalSwaps = 10 + 20 = 30
    // dt = (120 - 90) / 60.0 = 0.5 seconds
    // velocity = 30 / 0.5 = 60.0 swaps/sec
    EXPECT_FLOAT_EQ(tracker.velocity(pos), 60.0f);
}

// 10. Low positive velocity below threshold returns true
TEST_F(ChangeVelocityTrackerTest, IsSettlingTrue) {
    ChangeVelocityConfig cfg;
    cfg.ringSize = 8;
    cfg.windowFrames = 120;
    cfg.settlingThreshold = 10.0f;
    ChangeVelocityTracker tracker(cfg);
    fabric::ChunkCoord pos{0, 0, 0};

    // Two entries: 1 swap over 2 seconds = 0.5 swaps/sec (< 10.0)
    tracker.record(pos, 0, 0);
    tracker.record(pos, 1, 120);

    float v = tracker.velocity(pos);
    EXPECT_GT(v, 0.0f);
    EXPECT_LE(v, 10.0f);
    EXPECT_TRUE(tracker.isSettling(pos));
}

// 11. Zero velocity returns false (settled, not settling)
TEST_F(ChangeVelocityTrackerTest, IsSettlingFalseZero) {
    ChangeVelocityConfig cfg;
    cfg.ringSize = 8;
    cfg.windowFrames = 120;
    cfg.settlingThreshold = 10.0f;
    ChangeVelocityTracker tracker(cfg);
    fabric::ChunkCoord pos{0, 0, 0};

    // Two entries with zero swaps = velocity 0
    tracker.record(pos, 0, 0);
    tracker.record(pos, 0, 120);

    EXPECT_FLOAT_EQ(tracker.velocity(pos), 0.0f);
    EXPECT_FALSE(tracker.isSettling(pos));
}

// 12. High velocity returns false (active, not settling)
TEST_F(ChangeVelocityTrackerTest, IsSettlingFalseHigh) {
    ChangeVelocityConfig cfg;
    cfg.ringSize = 8;
    cfg.windowFrames = 120;
    cfg.settlingThreshold = 10.0f;
    ChangeVelocityTracker tracker(cfg);
    fabric::ChunkCoord pos{0, 0, 0};

    // 1000 swaps over 2 seconds = 500 swaps/sec (>> 10.0)
    tracker.record(pos, 0, 0);
    tracker.record(pos, 1000, 120);

    float v = tracker.velocity(pos);
    EXPECT_GT(v, 10.0f);
    EXPECT_FALSE(tracker.isSettling(pos));
}

// 13. remove() erases chunk data
TEST_F(ChangeVelocityTrackerTest, RemoveClearsRing) {
    auto tracker = makeTracker();
    fabric::ChunkCoord pos{1, 2, 3};

    tracker.record(pos, 50, 100);
    EXPECT_NE(tracker.history(pos), nullptr);
    EXPECT_GT(tracker.velocity(pos), 0.0f);

    tracker.remove(pos);
    EXPECT_EQ(tracker.history(pos), nullptr);
    EXPECT_FLOAT_EQ(tracker.velocity(pos), 0.0f);
}

// 14. clear() empties all rings
TEST_F(ChangeVelocityTrackerTest, ClearRemovesAll) {
    auto tracker = makeTracker();

    tracker.record({0, 0, 0}, 10, 1);
    tracker.record({1, 1, 1}, 20, 2);
    tracker.record({2, 2, 2}, 30, 3);

    EXPECT_NE(tracker.history({0, 0, 0}), nullptr);
    EXPECT_NE(tracker.history({1, 1, 1}), nullptr);
    EXPECT_NE(tracker.history({2, 2, 2}), nullptr);

    tracker.clear();

    EXPECT_EQ(tracker.history({0, 0, 0}), nullptr);
    EXPECT_EQ(tracker.history({1, 1, 1}), nullptr);
    EXPECT_EQ(tracker.history({2, 2, 2}), nullptr);
    EXPECT_FLOAT_EQ(tracker.velocity({0, 0, 0}), 0.0f);
}
