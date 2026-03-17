#pragma once

#include <cstring>
#include <deque>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>

namespace fabric {

/// Domain-specific snapshot provider. Registered with Timeline to participate
/// in snapshot/restore operations. Providers persist domain state outside
/// the in-memory TimeState (which is sized for small entity blobs, not bulk
/// data like chunk grids).
///
/// Lifecycle: the registrant owns the provider and must call removeProvider()
/// before destroying the provider.
class SnapshotProvider {
  public:
    virtual ~SnapshotProvider() = default;

    /// Called during Timeline::createSnapshot(). Persist domain state and
    /// return a token that can restore it later.
    virtual int64_t onCreateSnapshot(double timelineTime) = 0;

    /// Called during Timeline::restoreSnapshot(). Restore domain state
    /// to the point identified by snapshotToken.
    virtual void onRestoreSnapshot(int64_t snapshotToken) = 0;

    /// Stable name used as key when storing tokens in TimeState.
    /// Must be unique across all registered providers.
    virtual const char* providerName() const = 0;
};

class TimeState {
  public:
    using EntityID = std::string;

    TimeState();
    explicit TimeState(double timestamp);

    template <typename StateType> void setEntityState(const EntityID& entityId, const StateType& state) {
        static_assert(std::is_trivially_copyable_v<StateType>,
                      "StateType must be trivially copyable for memcpy serialization");
        std::vector<uint8_t> buffer(sizeof(StateType));
        std::memcpy(buffer.data(), &state, sizeof(StateType));
        entityStates_[entityId] = std::move(buffer);
    }

    template <typename StateType> std::optional<StateType> getEntityState(const EntityID& entityId) const {
        static_assert(std::is_trivially_copyable_v<StateType>,
                      "StateType must be trivially copyable for memcpy serialization");
        auto it = entityStates_.find(entityId);
        if (it == entityStates_.end() || it->second.size() < sizeof(StateType)) {
            return std::nullopt;
        }
        StateType result;
        std::memcpy(&result, it->second.data(), sizeof(StateType));
        return result;
    }

    double getTimestamp() const;
    TimeState(const TimeState&) = default;
    TimeState& operator=(const TimeState&) = default;
    TimeState(TimeState&&) = default;
    TimeState& operator=(TimeState&&) = default;

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
    static constexpr size_t K_MAX_HISTORY_SIZE = 100;

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

    void addProvider(SnapshotProvider* provider);
    void removeProvider(SnapshotProvider* provider);

    Timeline(const Timeline&) = delete;
    Timeline& operator=(const Timeline&) = delete;

  private:
    TimeState createSnapshotLocked() const;
    void restoreSnapshotLocked(const TimeState& state);
    double currentTime_;
    double globalTimeScale_;
    bool isPaused_;

    bool automaticSnapshots_;
    double snapshotInterval_;
    double snapshotCounter_;
    std::deque<TimeState> history_;

    std::vector<std::unique_ptr<TimeRegion>> regions_;
    std::vector<SnapshotProvider*> providers_;
    mutable std::mutex mutex_;
};

} // namespace fabric

// --- Temporal ops-as-values (Wave 5a4) ---

#include "fabric/fx/Error.hh"
#include "fabric/fx/Never.hh"
#include "fabric/fx/OneOf.hh"

namespace fabric::temporal::ops {

/// Dilate time within a spherical region.
struct DilateTime {
    float centerX, centerY, centerZ;
    float radius;
    float targetScale;
    float duration;

    static constexpr bool K_IS_SYNC = false;
    using Returns = void;
    using Errors = fx::TypeList<fx::Never>;
};

/// Create a timeline snapshot, returning its index.
struct CreateSnapshot {
    static constexpr bool K_IS_SYNC = false;
    using Returns = int;
    using Errors = fx::TypeList<fx::Never>;
};

/// Restore a previously created snapshot by index.
struct RestoreSnapshot {
    int index;

    static constexpr bool K_IS_SYNC = false;
    using Returns = void;
    using Errors = fx::TypeList<fx::StateError>;
};

} // namespace fabric::temporal::ops
