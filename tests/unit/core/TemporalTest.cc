#include "fabric/core/Temporal.hh"
#include "fabric/utils/Testing.hh"
#include "fabric/utils/ErrorHandling.hh"
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <memory>
#include <string>
#include <vector>

using namespace fabric;
using ::testing::_;
using ::testing::Return;

// Mock TimeBehavior for testing
class MockTimeBehavior : public TimeBehavior {
public:
    MOCK_METHOD(void, onTimeUpdate, (double deltaTime), (override));
    MOCK_METHOD(std::vector<uint8_t>, createSnapshot, (), (const, override));
    MOCK_METHOD(void, restoreSnapshot, (const std::vector<uint8_t>& data), (override));
};

// Concrete TimeBehavior for testing
class TestTimeBehavior : public TimeBehavior {
public:
    TestTimeBehavior(double& value) : value_(value), updateCount_(0) {}
    
    void onTimeUpdate(double deltaTime) override {
        value_ += deltaTime;
        updateCount_++;
    }
    
    std::vector<uint8_t> createSnapshot() const override {
        std::vector<uint8_t> data(sizeof(double));
        memcpy(data.data(), &value_, sizeof(double));
        return data;
    }
    
    void restoreSnapshot(const std::vector<uint8_t>& data) override {
        if (data.size() >= sizeof(double)) {
            memcpy(&value_, data.data(), sizeof(double));
        }
    }
    
    int getUpdateCount() const {
        return updateCount_;
    }
    
private:
    double& value_;
    int updateCount_;
};

class TemporalTest : public ::testing::Test {
protected:
    void SetUp() override {
        Timeline::reset();
    }

    void TearDown() override {
        Timeline::reset();
    }

    double testValue = 0.0;
};

TEST_F(TemporalTest, TimeStateBasics) {
    TimeState state(10.0);
    
    // Test timestamp
    EXPECT_DOUBLE_EQ(state.getTimestamp(), 10.0);
    
    // Test entity state storage and retrieval
    struct TestState {
        int intValue;
        float floatValue;
        
        bool operator==(const TestState& other) const {
            return intValue == other.intValue && 
                   std::abs(floatValue - other.floatValue) < 1e-5f;
        }
    };
    
    TestState originalState = {42, 3.14f};
    TimeState::EntityID entityId = "entity1";
    
    // Store the state
    state.setEntityState(entityId, originalState);
    
    // Retrieve the state
    auto retrievedState = state.getEntityState<TestState>(entityId);
    EXPECT_TRUE(retrievedState.has_value());
    EXPECT_EQ(retrievedState.value(), originalState);
    
    // Test missing state
    auto missingState = state.getEntityState<TestState>("nonexistent");
    EXPECT_FALSE(missingState.has_value());
}

TEST_F(TemporalTest, TimeStateDiff) {
    TimeState state1(10.0);
    TimeState state2(20.0);
    
    // Store states for entities
    state1.setEntityState("entity1", 10);
    state1.setEntityState("entity2", 20);
    state1.setEntityState("entity3", 30);
    
    state2.setEntityState("entity1", 10); // Same as state1
    state2.setEntityState("entity2", 25); // Different from state1
    state2.setEntityState("entity4", 40); // Not in state1
    
    // Compute differences
    auto diff = state1.diff(state2);
    
    // entity1 should be the same in both states
    auto it1 = diff.find("entity1");
    EXPECT_TRUE(it1 == diff.end() || it1->second == false);
    
    // entity2 should be different
    auto it2 = diff.find("entity2");
    EXPECT_TRUE(it2 != diff.end() && it2->second == true);
    
    // entity3 only exists in state1
    auto it3 = diff.find("entity3");
    EXPECT_TRUE(it3 != diff.end() && it3->second == false);
    
    // entity4 only exists in state2
    auto it4 = diff.find("entity4");
    EXPECT_TRUE(it4 != diff.end() && it4->second == false);
}

TEST_F(TemporalTest, TimeStateClone) {
    TimeState state(10.0);
    
    // Store state for an entity
    state.setEntityState("entity1", 42);
    
    // Clone the state
    std::unique_ptr<TimeState> clone = state.clone();
    
    // Check timestamp
    EXPECT_DOUBLE_EQ(clone->getTimestamp(), 10.0);
    
    // Check entity state
    auto value = clone->getEntityState<int>("entity1");
    EXPECT_TRUE(value.has_value());
    EXPECT_EQ(value.value(), 42);
}

TEST_F(TemporalTest, TimeRegionBasics) {
    TimeRegion region(2.0); // 2x time scale
    
    // Test time scale
    EXPECT_DOUBLE_EQ(region.getTimeScale(), 2.0);
    
    // Test time scale setting
    region.setTimeScale(0.5);
    EXPECT_DOUBLE_EQ(region.getTimeScale(), 0.5);
    
    // Test updating with time scale
    double worldDeltaTime = 1.0;
    double expectedRegionDeltaTime = worldDeltaTime * 0.5;
    
    // Update a few times and check that local time advances correctly
    for (int i = 0; i < 5; i++) {
        region.update(worldDeltaTime);
    }
    
    // We can't directly check local time, but we could set up a TimeBehavior
    // to verify the time scale is applied correctly
}

TEST_F(TemporalTest, TimeRegionSnapshot) {
    TimeRegion region;

    TimeState state = region.createSnapshot();
    EXPECT_EQ(state.getTimestamp(), 0.0);

    TimeState newState(10.0);
    region.restoreSnapshot(newState);

    // After restore, local time should match the snapshot
    TimeState restored = region.createSnapshot();
    EXPECT_DOUBLE_EQ(restored.getTimestamp(), 10.0);
}

TEST_F(TemporalTest, TimelineBasics) {
    Timeline& timeline = Timeline::instance();
    
    // Test initial state
    EXPECT_DOUBLE_EQ(timeline.getCurrentTime(), 0.0);
    EXPECT_DOUBLE_EQ(timeline.getGlobalTimeScale(), 1.0);
    EXPECT_FALSE(timeline.isPaused());
    
    // Test updating timeline
    timeline.update(1.0);
    EXPECT_DOUBLE_EQ(timeline.getCurrentTime(), 1.0);
    
    // Test global time scale
    timeline.setGlobalTimeScale(2.0);
    EXPECT_DOUBLE_EQ(timeline.getGlobalTimeScale(), 2.0);
    
    timeline.update(1.0);
    EXPECT_DOUBLE_EQ(timeline.getCurrentTime(), 3.0); // 1.0 + (1.0 * 2.0)
    
    // Test pausing
    timeline.pause();
    EXPECT_TRUE(timeline.isPaused());
    
    timeline.update(1.0);
    EXPECT_DOUBLE_EQ(timeline.getCurrentTime(), 3.0); // Should not change when paused
    
    // Test resuming
    timeline.resume();
    EXPECT_FALSE(timeline.isPaused());
    
    timeline.update(1.0);
    EXPECT_DOUBLE_EQ(timeline.getCurrentTime(), 5.0); // 3.0 + (1.0 * 2.0)
}

TEST_F(TemporalTest, TimelineRegions) {
    Timeline& timeline = Timeline::instance();
    
    // Create a time region
    TimeRegion* region = timeline.createRegion(0.5); // 0.5x time scale
    EXPECT_NE(region, nullptr);
    
    // Update the timeline
    timeline.update(2.0);
    
    // The region should have updated with scaled time, but we have no direct way
    // to check the internal state of the region from the public API
    
    // Remove the region
    timeline.removeRegion(region);
    
    // Create multiple regions with different time scales
    TimeRegion* fastRegion = timeline.createRegion(2.0);
    TimeRegion* slowRegion = timeline.createRegion(0.5);
    
    // Update the timeline
    timeline.update(1.0);
    
    // Again, we can't directly check the region states, but they should be updated
    // with their respective time scales
}

TEST_F(TemporalTest, TimelineSnapshots) {
    Timeline& timeline = Timeline::instance();
    
    // Create a snapshot
    TimeState snapshot = timeline.createSnapshot();
    EXPECT_DOUBLE_EQ(snapshot.getTimestamp(), timeline.getCurrentTime());
    
    // Advance time
    timeline.update(10.0);
    EXPECT_DOUBLE_EQ(timeline.getCurrentTime(), 10.0);
    
    // Restore snapshot
    timeline.restoreSnapshot(snapshot);
    EXPECT_DOUBLE_EQ(timeline.getCurrentTime(), 0.0);
}

TEST_F(TemporalTest, TimelineAutomaticSnapshots) {
    Timeline& timeline = Timeline::instance();
    
    // Enable automatic snapshots every 1.0 time units
    timeline.setAutomaticSnapshots(true, 1.0);
    
    // Update timeline multiple times
    timeline.update(0.6); // Not enough for snapshot
    EXPECT_EQ(timeline.getHistory().size(), 0);
    
    timeline.update(0.5); // Should trigger snapshot
    EXPECT_EQ(timeline.getHistory().size(), 1);
    
    timeline.update(2.5); // Should trigger 2 more snapshots
    EXPECT_EQ(timeline.getHistory().size(), 3);
    
    // Test jumping to a snapshot
    double currentTime = timeline.getCurrentTime();
    EXPECT_TRUE(timeline.jumpToSnapshot(0)); // Jump to first snapshot
    EXPECT_LT(timeline.getCurrentTime(), currentTime);
    
    // Clear history
    timeline.clearHistory();
    EXPECT_EQ(timeline.getHistory().size(), 0);
}

TEST_F(TemporalTest, TimeBehaviorBasics) {
    // Create a time behavior that updates a value
    double value = 0.0;
    TestTimeBehavior behavior(value);
    
    // Update the behavior
    behavior.onTimeUpdate(1.0);
    EXPECT_DOUBLE_EQ(value, 1.0);
    EXPECT_EQ(behavior.getUpdateCount(), 1);
    
    behavior.onTimeUpdate(2.5);
    EXPECT_DOUBLE_EQ(value, 3.5); // 1.0 + 2.5
    EXPECT_EQ(behavior.getUpdateCount(), 2);
}

TEST_F(TemporalTest, TimeBehaviorSnapshotting) {
    // Create a time behavior that updates a value
    double value = 5.0;
    TestTimeBehavior behavior(value);
    
    // Create a snapshot
    std::vector<uint8_t> snapshot = behavior.createSnapshot();
    
    // Change the value
    behavior.onTimeUpdate(10.0);
    EXPECT_DOUBLE_EQ(value, 15.0);
    
    // Restore from snapshot
    behavior.restoreSnapshot(snapshot);
    EXPECT_DOUBLE_EQ(value, 5.0);
}

TEST_F(TemporalTest, InterpolatorBasics) {
    // Test double interpolation
    double start = 10.0;
    double end = 20.0;
    
    // Test various interpolation points
    EXPECT_DOUBLE_EQ(Interpolator<double>::lerp(start, end, 0.0), 10.0);
    EXPECT_DOUBLE_EQ(Interpolator<double>::lerp(start, end, 0.5), 15.0);
    EXPECT_DOUBLE_EQ(Interpolator<double>::lerp(start, end, 1.0), 20.0);
    
    // Test extrapolation
    EXPECT_DOUBLE_EQ(Interpolator<double>::lerp(start, end, -0.5), 5.0);
    EXPECT_DOUBLE_EQ(Interpolator<double>::lerp(start, end, 1.5), 25.0);
}

TEST_F(TemporalTest, TimelinePrediction) {
    Timeline& timeline = Timeline::instance();
    
    // Set current time
    timeline.update(10.0);
    EXPECT_DOUBLE_EQ(timeline.getCurrentTime(), 10.0);
    
    // Predict 5 seconds into the future
    TimeState futureState = timeline.predictFutureState(5.0);
    
    // The future state should have a timestamp 5 seconds ahead
    EXPECT_DOUBLE_EQ(futureState.getTimestamp(), 15.0);
    
    // The current time should remain unchanged
    EXPECT_DOUBLE_EQ(timeline.getCurrentTime(), 10.0);
}

TEST_F(TemporalTest, MakeTimeBehavior) {
    double value = 0.0;
    
    // Create a time behavior with lambdas
    auto behavior = makeTimeBehavior<double>(
        [&value](double deltaTime) { value += deltaTime; },              // Update
        [&value]() { return value; },                                    // GetState
        [&value](const double& newValue) { value = newValue; }           // SetState
    );
    
    // Update the behavior
    behavior->onTimeUpdate(5.0);
    EXPECT_DOUBLE_EQ(value, 5.0);
    
    // Create and restore a snapshot
    auto snapshot = behavior->createSnapshot();
    
    value = 10.0; // Change the value
    
    behavior->restoreSnapshot(snapshot);
    EXPECT_DOUBLE_EQ(value, 5.0); // Value should be restored
}