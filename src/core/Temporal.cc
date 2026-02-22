#include "fabric/core/Temporal.hh"
#include <algorithm>
#include <stdexcept>

namespace fabric {

// Initialize static members
std::unique_ptr<Timeline> Timeline::instance_ = nullptr;

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
TimeRegion::TimeRegion() : timeScale_(1.0), localTime_(0.0) {}

TimeRegion::TimeRegion(double timeScale) : timeScale_(timeScale), localTime_(0.0) {}

void TimeRegion::update(double worldDeltaTime) {
    // Apply time scale to the delta time
    double scaledDelta = worldDeltaTime * timeScale_;
    localTime_ += scaledDelta;
    
    // Update all entities in this region
    for (auto entity : entities_) {
        // For now, just a placeholder as we don't have the Entity class defined
        // entity->update(scaledDelta);
    }
}

double TimeRegion::getTimeScale() const {
    return timeScale_;
}

void TimeRegion::setTimeScale(double scale) {
    timeScale_ = scale;
}

void TimeRegion::addEntity(Entity* entity) {
    // Check if entity is already in this region
    if (std::find(entities_.begin(), entities_.end(), entity) == entities_.end()) {
        entities_.push_back(entity);
    }
}

void TimeRegion::removeEntity(Entity* entity) {
    auto it = std::find(entities_.begin(), entities_.end(), entity);
    if (it != entities_.end()) {
        entities_.erase(it);
    }
}

const std::vector<Entity*>& TimeRegion::getEntities() const {
    return entities_;
}

TimeState TimeRegion::createSnapshot() const {
    TimeState state(localTime_);
    
    // For each entity, capture its state
    // This is a placeholder since we don't have the Entity class defined
    
    return state;
}

void TimeRegion::restoreSnapshot(const TimeState& state) {
    // For each entity, restore its state from the snapshot
    // This is a placeholder since we don't have the Entity class defined
}

// Timeline implementation
Timeline::Timeline() :
    currentTime_(0.0),
    globalTimeScale_(1.0),
    isPaused_(false),
    automaticSnapshots_(false),
    snapshotInterval_(1.0),
    snapshotCounter_(0.0) {
}

void Timeline::update(double deltaTime) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (isPaused_) {
        return;
    }
    
    // Apply global time scale
    double scaledDelta = deltaTime * globalTimeScale_;
    currentTime_ += scaledDelta;
    
    // Create automatic snapshots for each elapsed interval
    if (automaticSnapshots_) {
        snapshotCounter_ += deltaTime;
        while (snapshotCounter_ >= snapshotInterval_) {
            history_.push_back(createSnapshot());
            snapshotCounter_ -= snapshotInterval_;

            if (history_.size() > 100) {
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
    }
}

TimeState Timeline::createSnapshot() const {
    TimeState state(currentTime_);
    
    // Combine snapshots from all regions
    for (const auto& region : regions_) {
        TimeState regionState = region->createSnapshot();
        
        // Merge region state into the global state
        // This is a placeholder since we need specifics of the merging process
    }
    
    return state;
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
    globalTimeScale_ = scale;
}

double Timeline::getGlobalTimeScale() const {
    return globalTimeScale_;
}

void Timeline::pause() {
    std::lock_guard<std::mutex> lock(mutex_);
    isPaused_ = true;
}

void Timeline::resume() {
    std::lock_guard<std::mutex> lock(mutex_);
    isPaused_ = false;
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
        return false;
    }

    restoreSnapshotLocked(history_[index]);
    return true;
}

TimeState Timeline::predictFutureState(double secondsAhead) const {
    // Simple prediction by cloning the current state
    // In a real implementation, this would use physics and motion information
    // to predict future positions and states
    
    TimeState futureState(currentTime_ + secondsAhead);
    
    // This is a placeholder for actual prediction logic
    // which would involve extrapolating motion, animation, etc.
    
    return futureState;
}

Timeline& Timeline::instance() {
    if (!instance_) {
        instance_ = std::make_unique<Timeline>();
    }
    return *instance_;
}

void Timeline::reset() {
    instance_.reset();
}

} // namespace fabric