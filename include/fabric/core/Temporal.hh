#ifndef FABRIC_CORE_TEMPORAL_HH
#define FABRIC_CORE_TEMPORAL_HH

#include <chrono>
#include <cstring>
#include <deque>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace fabric {

class TimeState {
  public:
    using EntityID = std::string;

    TimeState();
    explicit TimeState(double timestamp);

    template <typename StateType> void setEntityState(const EntityID& entityId, const StateType& state) {
        std::vector<uint8_t> buffer(sizeof(StateType));
        std::memcpy(buffer.data(), &state, sizeof(StateType));
        entityStates_[entityId] = std::move(buffer);
    }

    template <typename StateType> std::optional<StateType> getEntityState(const EntityID& entityId) const {
        auto it = entityStates_.find(entityId);
        if (it == entityStates_.end() || it->second.size() < sizeof(StateType)) {
            return std::nullopt;
        }
        StateType result;
        std::memcpy(&result, it->second.data(), sizeof(StateType));
        return result;
    }

    double getTimestamp() const;
    std::unique_ptr<TimeState> clone() const;

  private:
    double timestamp_;
    std::unordered_map<EntityID, std::vector<uint8_t>> entityStates_;
};

class TimeRegion {
  public:
    TimeRegion();
    explicit TimeRegion(double timeScale);

    void update(double worldDeltaTime);
    double getTimeScale() const;
    void setTimeScale(double scale);

    TimeState createSnapshot() const;
    void restoreSnapshot(const TimeState& state);

  private:
    double timeScale_;
    double localTime_;
};

class Timeline {
  public:
    Timeline();

    void update(double deltaTime);
    TimeRegion* createRegion(double timeScale = 1.0);
    void removeRegion(TimeRegion* region);

    TimeState createSnapshot() const;
    void restoreSnapshot(const TimeState& state);

    double getCurrentTime() const;
    void setGlobalTimeScale(double scale);
    double getGlobalTimeScale() const;

    void pause();
    void resume();
    bool isPaused() const;

    void setAutomaticSnapshots(bool enable, double interval = 1.0);
    const std::deque<TimeState>& getHistory() const;
    void clearHistory();
    bool jumpToSnapshot(size_t index);

    Timeline(const Timeline&) = delete;
    Timeline& operator=(const Timeline&) = delete;

  private:
    void restoreSnapshotLocked(const TimeState& state);
    double currentTime_;
    double globalTimeScale_;
    bool isPaused_;

    bool automaticSnapshots_;
    double snapshotInterval_;
    double snapshotCounter_;
    std::deque<TimeState> history_;

    std::vector<std::unique_ptr<TimeRegion>> regions_;
    mutable std::mutex mutex_;
};

} // namespace fabric

#endif // FABRIC_CORE_TEMPORAL_HH
