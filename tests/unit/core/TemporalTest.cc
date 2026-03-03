#include "fabric/core/Temporal.hh"
#include "fabric/utils/ErrorHandling.hh"
#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <vector>

using namespace fabric;

class TemporalTest : public ::testing::Test {};

TEST_F(TemporalTest, TimeStateBasics) {
    TimeState state(10.0);

    EXPECT_DOUBLE_EQ(state.getTimestamp(), 10.0);

    struct TestState {
        int intValue;
        float floatValue;

        bool operator==(const TestState& other) const {
            return intValue == other.intValue && std::abs(floatValue - other.floatValue) < 1e-5f;
        }
    };

    TestState originalState = {42, 3.14f};
    TimeState::EntityID entityId = "entity1";

    state.setEntityState(entityId, originalState);

    auto retrievedState = state.getEntityState<TestState>(entityId);
    EXPECT_TRUE(retrievedState.has_value());
    EXPECT_EQ(retrievedState.value(), originalState);

    auto missingState = state.getEntityState<TestState>("nonexistent");
    EXPECT_FALSE(missingState.has_value());
}

TEST_F(TemporalTest, TimeStateCopy) {
    TimeState state(10.0);

    state.setEntityState("entity1", 42);

    TimeState copy(state);

    EXPECT_DOUBLE_EQ(copy.getTimestamp(), 10.0);

    auto value = copy.getEntityState<int>("entity1");
    EXPECT_TRUE(value.has_value());
    EXPECT_EQ(value.value(), 42);
}

TEST_F(TemporalTest, TimeStateCopyIndependence) {
    TimeState state(10.0);
    state.setEntityState("entity1", 42);

    TimeState copy(state);

    copy.setEntityState("entity1", 100);

    auto originalValue = state.getEntityState<int>("entity1");
    EXPECT_EQ(originalValue.value(), 42);

    auto copyValue = copy.getEntityState<int>("entity1");
    EXPECT_EQ(copyValue.value(), 100);
}

TEST_F(TemporalTest, TimeRegionBasics) {
    TimeRegion region(2.0);

    EXPECT_DOUBLE_EQ(region.getTimeScale(), 2.0);

    region.setTimeScale(0.5);
    EXPECT_DOUBLE_EQ(region.getTimeScale(), 0.5);

    region.update(2.0);
    TimeState snapshot = region.createSnapshot();
    EXPECT_DOUBLE_EQ(snapshot.getTimestamp(), 1.0);

    TimeState newState(10.0);
    region.restoreSnapshot(newState);

    TimeState restored = region.createSnapshot();
    EXPECT_DOUBLE_EQ(restored.getTimestamp(), 10.0);
}

TEST_F(TemporalTest, TimelineBasics) {
    Timeline timeline;

    EXPECT_DOUBLE_EQ(timeline.getCurrentTime(), 0.0);
    EXPECT_DOUBLE_EQ(timeline.getGlobalTimeScale(), 1.0);
    EXPECT_FALSE(timeline.isPaused());

    timeline.update(1.0);
    EXPECT_DOUBLE_EQ(timeline.getCurrentTime(), 1.0);

    timeline.setGlobalTimeScale(2.0);
    EXPECT_DOUBLE_EQ(timeline.getGlobalTimeScale(), 2.0);

    timeline.update(1.0);
    EXPECT_DOUBLE_EQ(timeline.getCurrentTime(), 3.0);

    timeline.pause();
    EXPECT_TRUE(timeline.isPaused());

    timeline.update(1.0);
    EXPECT_DOUBLE_EQ(timeline.getCurrentTime(), 3.0);

    timeline.resume();
    EXPECT_FALSE(timeline.isPaused());

    timeline.update(1.0);
    EXPECT_DOUBLE_EQ(timeline.getCurrentTime(), 5.0);
}

TEST_F(TemporalTest, TimelineRegions) {
    Timeline timeline;

    TimeRegion* region = timeline.createRegion(0.5);
    EXPECT_NE(region, nullptr);

    timeline.update(2.0);

    timeline.removeRegion(region);

    TimeRegion* fastRegion = timeline.createRegion(2.0);
    TimeRegion* slowRegion = timeline.createRegion(0.5);

    EXPECT_NE(fastRegion, nullptr);
    EXPECT_NE(slowRegion, nullptr);

    timeline.update(1.0);
}

TEST_F(TemporalTest, TimelineSnapshots) {
    Timeline timeline;

    TimeState snapshot = timeline.createSnapshot();
    EXPECT_DOUBLE_EQ(snapshot.getTimestamp(), timeline.getCurrentTime());

    timeline.update(10.0);
    EXPECT_DOUBLE_EQ(timeline.getCurrentTime(), 10.0);

    timeline.restoreSnapshot(snapshot);
    EXPECT_DOUBLE_EQ(timeline.getCurrentTime(), 0.0);
}

TEST_F(TemporalTest, TimelineAutomaticSnapshots) {
    Timeline timeline;

    timeline.setAutomaticSnapshots(true, 1.0);

    timeline.update(0.6);
    EXPECT_EQ(timeline.getHistory().size(), 0);

    timeline.update(0.5);
    EXPECT_EQ(timeline.getHistory().size(), 1);

    timeline.update(2.5);
    EXPECT_EQ(timeline.getHistory().size(), 3);

    double currentTime = timeline.getCurrentTime();
    EXPECT_TRUE(timeline.jumpToSnapshot(0));
    EXPECT_LT(timeline.getCurrentTime(), currentTime);

    timeline.clearHistory();
    EXPECT_EQ(timeline.getHistory().size(), 0);
}

TEST_F(TemporalTest, TimelineSnapshotHistoryBounds) {
    Timeline timeline;

    timeline.setAutomaticSnapshots(true, 1.0);

    for (int i = 0; i < 150; ++i) {
        timeline.update(1.0);
    }

    EXPECT_LE(timeline.getHistory().size(), Timeline::kMaxHistorySize);
}

TEST_F(TemporalTest, TimelineJumpToSnapshotEdgeCases) {
    Timeline timeline;

    timeline.setAutomaticSnapshots(true, 1.0);
    timeline.update(3.0);

    EXPECT_FALSE(timeline.jumpToSnapshot(999));

    EXPECT_TRUE(timeline.jumpToSnapshot(2));
    EXPECT_EQ(timeline.getHistory().size(), 3);

    EXPECT_TRUE(timeline.jumpToSnapshot(0));
}

TEST_F(TemporalTest, TimelineWithPausedSnapshots) {
    Timeline timeline;

    timeline.setAutomaticSnapshots(true, 1.0);
    timeline.update(1.5);
    EXPECT_EQ(timeline.getHistory().size(), 1);

    timeline.pause();
    timeline.update(2.0);
    EXPECT_EQ(timeline.getHistory().size(), 1);

    timeline.resume();
    timeline.update(0.6);
    EXPECT_EQ(timeline.getHistory().size(), 2);
}

TEST_F(TemporalTest, TimelineMultiThreadedSnapshotRestore) {
    Timeline timeline;

    TimeState snapshot = timeline.createSnapshot();
    timeline.update(5.0);

    timeline.restoreSnapshot(snapshot);
    EXPECT_DOUBLE_EQ(timeline.getCurrentTime(), 0.0);

    timeline.update(3.0);
    TimeState snapshot2 = timeline.createSnapshot();
    EXPECT_DOUBLE_EQ(snapshot2.getTimestamp(), 3.0);
}

TEST_F(TemporalTest, TimelineRegionSnapshotPreservation) {
    Timeline timeline;

    TimeRegion* region = timeline.createRegion(2.0);
    timeline.update(2.0);

    TimeState beforeSnapshot = region->createSnapshot();

    timeline.update(3.0);

    TimeState afterSnapshot = region->createSnapshot();
    EXPECT_GT(afterSnapshot.getTimestamp(), beforeSnapshot.getTimestamp());

    timeline.pause();

    TimeRegion* pausedRegion = timeline.createRegion(0.5);
    timeline.update(1.0);

    TimeState pausedSnapshot = pausedRegion->createSnapshot();
    EXPECT_DOUBLE_EQ(pausedSnapshot.getTimestamp(), 0.0);

    timeline.resume();
    timeline.update(2.0);

    TimeState resumedSnapshot = pausedRegion->createSnapshot();
    EXPECT_DOUBLE_EQ(resumedSnapshot.getTimestamp(), 1.0);
}

TEST_F(TemporalTest, TimelineDynamicTimeScale) {
    Timeline timeline;

    timeline.update(1.0);
    EXPECT_DOUBLE_EQ(timeline.getCurrentTime(), 1.0);

    timeline.setGlobalTimeScale(0.5);
    timeline.update(2.0);
    EXPECT_DOUBLE_EQ(timeline.getCurrentTime(), 2.0);

    timeline.setGlobalTimeScale(3.0);
    timeline.update(1.0);
    EXPECT_DOUBLE_EQ(timeline.getCurrentTime(), 5.0);
}

TEST_F(TemporalTest, TimelineEntityStatePersistence) {
    Timeline timeline;

    TimeState snapshot = timeline.createSnapshot();

    struct EntityState {
        int health;
        float stamina;
    };

    EntityState state = {100, 50.0f};
    snapshot.setEntityState("player", state);

    timeline.restoreSnapshot(snapshot);

    auto restored = snapshot.getEntityState<EntityState>("player");
    EXPECT_TRUE(restored.has_value());
    EXPECT_EQ(restored->health, 100);
    EXPECT_FLOAT_EQ(restored->stamina, 50.0f);
}

// ============================================================
// Entity state tests (setEntityState / getEntityState)
// ============================================================

TEST_F(TemporalTest, EntityStateSetAndGet) {
    TimeState state(1.0);

    struct Position {
        float x, y, z;
    };

    Position pos{10.0f, 20.0f, 30.0f};
    state.setEntityState("entity1", pos);

    auto retrieved = state.getEntityState<Position>("entity1");
    EXPECT_TRUE(retrieved.has_value());
    EXPECT_FLOAT_EQ(retrieved->x, 10.0f);
    EXPECT_FLOAT_EQ(retrieved->y, 20.0f);
    EXPECT_FLOAT_EQ(retrieved->z, 30.0f);
}

TEST_F(TemporalTest, EntityStateGetMissingReturnsNullopt) {
    TimeState state(0.0);
    auto result = state.getEntityState<int>("nonexistent");
    EXPECT_FALSE(result.has_value());
}

TEST_F(TemporalTest, EntityStateOverwrite) {
    TimeState state(0.0);

    state.setEntityState("e1", 42);
    EXPECT_EQ(state.getEntityState<int>("e1").value(), 42);

    state.setEntityState("e1", 99);
    EXPECT_EQ(state.getEntityState<int>("e1").value(), 99);
}

TEST_F(TemporalTest, EntityStateMultipleEntities) {
    TimeState state(5.0);

    state.setEntityState("a", 1);
    state.setEntityState("b", 2);
    state.setEntityState("c", 3);

    EXPECT_EQ(state.getEntityState<int>("a").value(), 1);
    EXPECT_EQ(state.getEntityState<int>("b").value(), 2);
    EXPECT_EQ(state.getEntityState<int>("c").value(), 3);
}

TEST_F(TemporalTest, EntityStateDifferentTypes) {
    TimeState state(0.0);

    struct Stats {
        int hp;
        float speed;
    };

    state.setEntityState("player", Stats{100, 5.5f});
    state.setEntityState("score", 42);

    auto stats = state.getEntityState<Stats>("player");
    EXPECT_TRUE(stats.has_value());
    EXPECT_EQ(stats->hp, 100);
    EXPECT_FLOAT_EQ(stats->speed, 5.5f);

    auto score = state.getEntityState<int>("score");
    EXPECT_TRUE(score.has_value());
    EXPECT_EQ(score.value(), 42);
}

TEST_F(TemporalTest, EntityStateSizeMismatchReturnsNullopt) {
    TimeState state(0.0);

    // Store an int (4 bytes), try to retrieve a larger struct
    state.setEntityState("small", 42);

    struct LargeStruct {
        double a, b, c;
    };

    // Buffer is only sizeof(int) bytes, LargeStruct needs more
    auto result = state.getEntityState<LargeStruct>("small");
    EXPECT_FALSE(result.has_value());
}

TEST_F(TemporalTest, EntityStateCopyPreservesData) {
    TimeState original(10.0);
    original.setEntityState("e1", 100);
    original.setEntityState("e2", 200);

    TimeState copy(original);

    EXPECT_EQ(copy.getEntityState<int>("e1").value(), 100);
    EXPECT_EQ(copy.getEntityState<int>("e2").value(), 200);

    // Modify copy, original unchanged
    copy.setEntityState("e1", 999);
    EXPECT_EQ(original.getEntityState<int>("e1").value(), 100);
    EXPECT_EQ(copy.getEntityState<int>("e1").value(), 999);
}
