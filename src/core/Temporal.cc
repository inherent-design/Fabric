#include "fabric/core/Temporal.hh"
#include "fabric/core/Log.hh"
#include <algorithm>

namespace fabric {

// TimeState implementation
TimeState::TimeState() : timestamp_(0.0) {}

TimeState::TimeState(double timestamp) : timestamp_(timestamp) {}

double TimeState::getTimestamp() const {
    return timestamp_;
}

std::unordered_map<TimeState::EntityID, bool> TimeState::diff(const TimeState& other) const {
    std::unordered_map<EntityID, bool> result;
    
    // Check entities in this state
    for (const auto& [id, state] : entityStates_) {
        auto it = other.entityStates_.find(id);
        if (it == other.entityStates_.end()) {
            // Entity only exists in this state
            result[id] = false;
        } else if (state != it->second) {
            // Entity exists in both but with different states
            result[id] = true;
        }
    }
    
    // Check entities that only exist in the other state
    for (const auto& [id, state] : other.entityStates_) {
        if (entityStates_.find(id) == entityStates_.end()) {
            result[id] = false;
        }
    }
    
    return result;
}

std::unique_ptr<TimeState> TimeState::clone() const {
    auto clone = std::make_unique<TimeState>(timestamp_);
    clone->entityStates_ = entityStates_;
    return clone;
}

// TimeRegion implementation
TimeRegion::TimeRegion() : timeScale_(1.0), localTime_(0.0) {
    FABRIC_LOG_DEBUG("TimeRegion created with default scale 1.0");
}

TimeRegion::TimeRegion(double timeScale) : timeScale_(timeScale), localTime_(0.0) {
    FABRIC_LOG_DEBUG("TimeRegion created with scale {}", timeScale);
}

void TimeRegion::update(double worldDeltaTime) {
    double scaledDelta = worldDeltaTime * timeScale_;
    localTime_ += scaledDelta;
}

double TimeRegion::getTimeScale() const {
    return timeScale_;
}

void TimeRegion::setTimeScale(double scale) {
    FABRIC_LOG_DEBUG("TimeRegion scale changed from {} to {}", timeScale_, scale);
    timeScale_ = scale;
}

TimeState TimeRegion::createSnapshot() const {
    return TimeState(localTime_);
}

void TimeRegion::restoreSnapshot(const TimeState& state) {
    localTime_ = state.getTimestamp();
}

// Timeline implementation
Timeline::Timeline() :
    currentTime_(0.0),
    globalTimeScale_(1.0),
    isPaused_(false),
    automaticSnapshots_(false),
    snapshotInterval_(1.0),
    snapshotCounter_(0.0) {
    FABRIC_LOG_DEBUG("Timeline initialized");
}

void Timeline::update(double deltaTime) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (isPaused_) {
        return;
    }
    
    // Apply global time scale
    double scaledDelta = deltaTime * globalTimeScale_;
    currentTime_ += scaledDelta;
    
    // Accumulate real time and emit a snapshot for each elapsed interval.
    // Ring buffer caps at 100 entries to bound memory.
    if (automaticSnapshots_) {
        snapshotCounter_ += deltaTime;
        while (snapshotCounter_ >= snapshotInterval_) {
            history_.push_back(createSnapshot());
            snapshotCounter_ -= snapshotInterval_;

            if (history_.size() > 100) {
                FABRIC_LOG_WARN("Snapshot history full, discarding oldest entry");
                history_.pop_front();
            }
        }
    }
    
    // Update all time regions
    for (auto& region : regions_) {
        region->update(scaledDelta);
    }
}

TimeRegion* Timeline::createRegion(double timeScale) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto region = std::make_unique<TimeRegion>(timeScale);
    TimeRegion* result = region.get();
    regions_.push_back(std::move(region));
    FABRIC_LOG_DEBUG("Timeline: created region (scale={}, total={})", timeScale, regions_.size());
    return result;
}

void Timeline::removeRegion(TimeRegion* region) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = std::find_if(regions_.begin(), regions_.end(),
                         [region](const std::unique_ptr<TimeRegion>& r) {
                             return r.get() == region;
                         });

    if (it != regions_.end()) {
        regions_.erase(it);
        FABRIC_LOG_DEBUG("Timeline: removed region (remaining={})", regions_.size());
    }
}

TimeState Timeline::createSnapshot() const {
    return TimeState(currentTime_);
}

void Timeline::restoreSnapshot(const TimeState& state) {
    std::lock_guard<std::mutex> lock(mutex_);
    restoreSnapshotLocked(state);
}

void Timeline::restoreSnapshotLocked(const TimeState& state) {
    currentTime_ = state.getTimestamp();

    for (auto& region : regions_) {
        region->restoreSnapshot(state);
    }
}

double Timeline::getCurrentTime() const {
    return currentTime_;
}

void Timeline::setGlobalTimeScale(double scale) {
    std::lock_guard<std::mutex> lock(mutex_);
    FABRIC_LOG_DEBUG("Timeline: global time scale {} -> {}", globalTimeScale_, scale);
    globalTimeScale_ = scale;
}

double Timeline::getGlobalTimeScale() const {
    return globalTimeScale_;
}

void Timeline::pause() {
    std::lock_guard<std::mutex> lock(mutex_);
    isPaused_ = true;
    FABRIC_LOG_DEBUG("Timeline paused at t={}", currentTime_);
}

void Timeline::resume() {
    std::lock_guard<std::mutex> lock(mutex_);
    isPaused_ = false;
    FABRIC_LOG_DEBUG("Timeline resumed at t={}", currentTime_);
}

bool Timeline::isPaused() const {
    return isPaused_;
}

void Timeline::setAutomaticSnapshots(bool enable, double interval) {
    std::lock_guard<std::mutex> lock(mutex_);
    automaticSnapshots_ = enable;
    snapshotInterval_ = interval;
    snapshotCounter_ = 0.0;
}

const std::deque<TimeState>& Timeline::getHistory() const {
    return history_;
}

void Timeline::clearHistory() {
    std::lock_guard<std::mutex> lock(mutex_);
    history_.clear();
}

bool Timeline::jumpToSnapshot(size_t index) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (index >= history_.size()) {
        FABRIC_LOG_WARN("jumpToSnapshot: index {} out of range (history size={})", index, history_.size());
        return false;
    }

    FABRIC_LOG_DEBUG("Timeline: jumping to snapshot {} (t={})", index, history_[index].getTimestamp());
    restoreSnapshotLocked(history_[index]);
    return true;
}

TimeState Timeline::predictFutureState(double secondsAhead) const {
    return TimeState(currentTime_ + secondsAhead);
}

} // namespace fabric