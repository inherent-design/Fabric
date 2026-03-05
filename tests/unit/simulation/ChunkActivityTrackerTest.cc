#include "fabric/simulation/ChunkActivityTracker.hh"
#include <gtest/gtest.h>

using namespace fabric::simulation;

class ChunkActivityTrackerTest : public ::testing::Test {
  protected:
    ChunkActivityTracker tracker;
};

// 1. Untracked = Sleeping
TEST_F(ChunkActivityTrackerTest, DefaultStateSleeping) {
    EXPECT_EQ(tracker.getState({10, 20, 30}), ChunkState::Sleeping);
}

// 2. Active round-trip
TEST_F(ChunkActivityTrackerTest, SetStateActiveAndQuery) {
    ChunkPos pos{1, 2, 3};
    tracker.setState(pos, ChunkState::Active);
    EXPECT_EQ(tracker.getState(pos), ChunkState::Active);
}

// 3. notifyBoundaryChange: Sleeping -> BoundaryDirty
TEST_F(ChunkActivityTrackerTest, SleepingToBoundaryDirty) {
    ChunkPos pos{0, 0, 0};
    tracker.notifyBoundaryChange(pos);
    EXPECT_EQ(tracker.getState(pos), ChunkState::BoundaryDirty);
}

// 4. Active becomes BoundaryDirty on notify (so it re-meshes)
TEST_F(ChunkActivityTrackerTest, ActiveRemainsOnNotify) {
    ChunkPos pos{0, 0, 0};
    tracker.setState(pos, ChunkState::Active);
    tracker.notifyBoundaryChange(pos);
    EXPECT_EQ(tracker.getState(pos), ChunkState::BoundaryDirty);
}

// 5. resolve(false) -> Sleeping
TEST_F(ChunkActivityTrackerTest, BoundaryDirtyResolveToSleeping) {
    ChunkPos pos{0, 0, 0};
    tracker.setState(pos, ChunkState::BoundaryDirty);
    tracker.resolveBoundaryDirty(pos, false);
    EXPECT_EQ(tracker.getState(pos), ChunkState::Sleeping);
}

// 6. resolve(true) -> Active
TEST_F(ChunkActivityTrackerTest, BoundaryDirtyResolveToActive) {
    ChunkPos pos{0, 0, 0};
    tracker.setState(pos, ChunkState::BoundaryDirty);
    tracker.resolveBoundaryDirty(pos, true);
    EXPECT_EQ(tracker.getState(pos), ChunkState::Active);
}

// 7. (0,0,0) = bit 0; (31,31,31) = bit 63
TEST_F(ChunkActivityTrackerTest, SubRegionBitmaskCornerBits) {
    ChunkPos pos{0, 0, 0};

    tracker.markSubRegionActive(pos, 0, 0, 0);
    uint64_t mask = tracker.getSubRegionMask(pos);
    EXPECT_EQ(mask, uint64_t{1} << 0);

    tracker.clearSubRegionMask(pos);
    tracker.markSubRegionActive(pos, 31, 31, 31);
    mask = tracker.getSubRegionMask(pos);
    // (31>>3)=3, (31>>3)*4=12, (31>>3)*16=48 -> bit 3+12+48=63
    EXPECT_EQ(mask, uint64_t{1} << 63);
}

// 8. Two distant voxels = 2 bits
TEST_F(ChunkActivityTrackerTest, SubRegionBitmaskMultipleBits) {
    ChunkPos pos{0, 0, 0};
    tracker.markSubRegionActive(pos, 0, 0, 0);    // bit 0
    tracker.markSubRegionActive(pos, 31, 31, 31); // bit 63
    uint64_t mask = tracker.getSubRegionMask(pos);
    EXPECT_EQ(mask, (uint64_t{1} << 0) | (uint64_t{1} << 63));
}

// 9. Priority-ascending order
TEST_F(ChunkActivityTrackerTest, CollectSortedByPriority) {
    tracker.setReferencePoint(0, 0, 0);

    // Close chunk
    tracker.setState({0, 0, 0}, ChunkState::Active);
    // Far chunk
    tracker.setState({100, 100, 100}, ChunkState::Active);
    // Medium chunk
    tracker.setState({5, 0, 0}, ChunkState::Active);

    auto active = tracker.collectActiveChunks();
    ASSERT_GE(active.size(), 3u);

    for (size_t i = 1; i < active.size(); ++i) {
        EXPECT_LE(static_cast<uint8_t>(active[i - 1].priority), static_cast<uint8_t>(active[i].priority));
    }

    // First should be Immediate (chunk 0,0,0 is distance 0)
    EXPECT_EQ(active[0].priority, SimPriority::Immediate);
}

// 10. budgetCap=5 from 20 active = 5 returned
TEST_F(ChunkActivityTrackerTest, BudgetCapLimits) {
    tracker.setReferencePoint(0, 0, 0);

    for (int i = 0; i < 20; ++i) {
        tracker.setState({i, 0, 0}, ChunkState::Active);
    }

    auto active = tracker.collectActiveChunks(5);
    EXPECT_EQ(active.size(), 5u);
}

// 11. Sleeping + mask = 0
TEST_F(ChunkActivityTrackerTest, PutToSleepClearsMask) {
    ChunkPos pos{0, 0, 0};
    tracker.setState(pos, ChunkState::Active);
    tracker.markSubRegionActive(pos, 4, 4, 4);
    EXPECT_NE(tracker.getSubRegionMask(pos), 0u);

    tracker.putToSleep(pos);
    EXPECT_EQ(tracker.getState(pos), ChunkState::Sleeping);
    EXPECT_EQ(tracker.getSubRegionMask(pos), 0u);
}

// 12. Sleeping absent from results
TEST_F(ChunkActivityTrackerTest, CollectExcludesSleeping) {
    tracker.setState({0, 0, 0}, ChunkState::Sleeping);
    tracker.setState({1, 0, 0}, ChunkState::Active);
    tracker.setState({2, 0, 0}, ChunkState::Sleeping);

    auto active = tracker.collectActiveChunks();
    EXPECT_EQ(active.size(), 1u);
    EXPECT_EQ(active[0].pos.x, 1);
}
