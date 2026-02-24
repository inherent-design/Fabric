#ifndef FABRIC_CORE_TEMPORAL_HH
#define FABRIC_CORE_TEMPORAL_HH

#include <chrono>
#include <cstring> // For memcpy
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace fabric {

/**
 * @brief TimeState captures the state of a timeline at a specific moment
 *
 * TimeState is used for creating snapshots of system state that can be
 * restored later, enabling time manipulation and debugging.
 */
class TimeState {
  public:
    using EntityID = std::string;
    using Timestamp = std::chrono::steady_clock::time_point;

    TimeState();
    explicit TimeState(double timestamp);

    /** Add an entity's state to this time state */
    template <typename StateType> void setEntityState(const EntityID& entityId, const StateType& state) {
        // Serialize the state to bytes
        entityStates_[entityId] = serialize(state);
    }

    /** Retrieve an entity's state from this time state */
    template <typename StateType> std::optional<StateType> getEntityState(const EntityID& entityId) const {
        auto it = entityStates_.find(entityId);
        if (it == entityStates_.end()) {
            return std::nullopt;
        }

        return deserialize<StateType>(it->second);
    }

    /** Compare this time state with another and return differences */
    std::unordered_map<EntityID, bool> diff(const TimeState& other) const;

    /** Get the timestamp of this state */
    double getTimestamp() const;

    /** Clone this time state */
    std::unique_ptr<TimeState> clone() const;

  private:
    double timestamp_;
    std::unordered_map<EntityID, std::vector<uint8_t>> entityStates_;

    // Helper functions for serialization
    template <typename T> static std::vector<uint8_t> serialize(const T& data) {
        std::vector<uint8_t> buffer(sizeof(T));
        std::memcpy(buffer.data(), &data, sizeof(T));
        return buffer;
    }

    template <typename T> static T deserialize(const std::vector<uint8_t>& data) {
        T result;
        if (data.size() >= sizeof(T)) {
            std::memcpy(&result, data.data(), sizeof(T));
        }
        return result;
    }
};

/**
 * @brief TimeRegion represents a region of space that can have its own time flow
 *
 * Different regions can progress at different rates, allowing for localized
 * time manipulation.
 */
class TimeRegion {
  public:
    TimeRegion();
    explicit TimeRegion(double timeScale);

    /** Update this region with the given global delta time */
    void update(double worldDeltaTime);

    /** Get the time scale of this region */
    double getTimeScale() const;

    /** Set the time scale of this region */
    void setTimeScale(double scale);

    /** Create a snapshot capturing this region's local time */
    TimeState createSnapshot() const;

    /** Restore local time from a snapshot */
    void restoreSnapshot(const TimeState& state);

  private:
    double timeScale_;
    double localTime_;
};

/**
 * @brief TimeBehavior is an interface for objects that need time-based updates
 */
class TimeBehavior {
  public:
    virtual ~TimeBehavior() = default;

    /** Update this behavior with the given delta time */
    virtual void onTimeUpdate(double deltaTime) = 0;

    /** Create a state snapshot of this behavior */
    virtual std::vector<uint8_t> createSnapshot() const = 0;

    /** Restore from a state snapshot */
    virtual void restoreSnapshot(const std::vector<uint8_t>& data) = 0;
};

/**
 * @brief Timeline manages time flow and provides time manipulation capabilities
 */
class Timeline {
  public:
    Timeline();

    /** Update the timeline with the given real-world delta time */
    void update(double deltaTime);

    /** Create a new time region with the given scale */
    TimeRegion* createRegion(double timeScale = 1.0);

    /** Remove a time region */
    void removeRegion(TimeRegion* region);

    /** Create a snapshot of the entire timeline */
    TimeState createSnapshot() const;

    /** Restore a previously created snapshot */
    void restoreSnapshot(const TimeState& state);

    /** Get the current timeline time */
    double getCurrentTime() const;

    /** Set the global time scale factor */
    void setGlobalTimeScale(double scale);

    /** Get the global time scale factor */
    double getGlobalTimeScale() const;

    /** Pause the timeline */
    void pause();

    /** Resume the timeline */
    void resume();

    /** Check if the timeline is paused */
    bool isPaused() const;

    /** Enable/disable automatic snapshots */
    void setAutomaticSnapshots(bool enable, double interval = 1.0);

    /** Access the history of automatic snapshots */
    const std::deque<TimeState>& getHistory() const;

    /** Clear snapshot history */
    void clearHistory();

    /** Jump to a specific point in the snapshot history */
    bool jumpToSnapshot(size_t index);

    /** Create a prediction of future state */
    TimeState predictFutureState(double secondsAhead) const;

  private:
    /** Restore snapshot without acquiring mutex. Caller must hold mutex_. */
    void restoreSnapshotLocked(const TimeState& state);
    double currentTime_;
    double globalTimeScale_;
    bool isPaused_;

    // Snapshots
    bool automaticSnapshots_;
    double snapshotInterval_;
    double snapshotCounter_;
    std::deque<TimeState> history_;

    // Regions
    std::vector<std::unique_ptr<TimeRegion>> regions_;

    std::mutex mutex_;
};

/**
 * @brief Utility for time interpolation between states
 */
template <typename T> class Interpolator {
  public:
    static T lerp(const T& a, const T& b, double t);
};

// Specialized interpolators for common types
template <> class Interpolator<double> {
  public:
    static double lerp(const double& a, const double& b, double t) { return a + (b - a) * t; }
};

// Helper functions
/**
 * @brief Creates a time behavior from a lambda function
 */
template <typename StateType>
std::unique_ptr<TimeBehavior> makeTimeBehavior(std::function<void(double)> updateFunc,
                                               std::function<StateType()> getStateFunc,
                                               std::function<void(const StateType&)> setStateFunc) {
    // Create a lambda-based TimeBehavior implementation
    class LambdaTimeBehavior : public TimeBehavior {
      public:
        LambdaTimeBehavior(std::function<void(double)> updateFunc, std::function<StateType()> getStateFunc,
                           std::function<void(const StateType&)> setStateFunc)
            : updateFunc_(updateFunc), getStateFunc_(getStateFunc), setStateFunc_(setStateFunc) {}

        void onTimeUpdate(double deltaTime) override { updateFunc_(deltaTime); }

        std::vector<uint8_t> createSnapshot() const override {
            StateType state = getStateFunc_();
            std::vector<uint8_t> data(sizeof(StateType));
            std::memcpy(data.data(), &state, sizeof(StateType));
            return data;
        }

        void restoreSnapshot(const std::vector<uint8_t>& data) override {
            if (data.size() >= sizeof(StateType)) {
                StateType state;
                std::memcpy(&state, data.data(), sizeof(StateType));
                setStateFunc_(state);
            }
        }

      private:
        std::function<void(double)> updateFunc_;
        std::function<StateType()> getStateFunc_;
        std::function<void(const StateType&)> setStateFunc_;
    };

    return std::make_unique<LambdaTimeBehavior>(updateFunc, getStateFunc, setStateFunc);
};

} // namespace fabric

#endif // FABRIC_CORE_TEMPORAL_HH
