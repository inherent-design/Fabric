#include "fabric/core/Temporal.hh"
#include <algorithm>

namespace fabric {

TimeState::TimeState() : timestamp_(0.0) {}

TimeState::TimeState(double timestamp) : timestamp_(timestamp) {}

double TimeState::getTimestamp() const {
    return timestamp_;
}

TimeRegion::TimeRegion() : timeScale_(1.0), localTime_(0.0) {}

TimeRegion::TimeRegion(double timeScale) : timeScale_(timeScale), localTime_(0.0) {}

void TimeRegion::update(double worldDeltaTime) {
    localTime_ += worldDeltaTime * timeScale_;
}

double TimeRegion::getTimeScale() const {
    return timeScale_;
}

void TimeRegion::setTimeScale(double scale) {
    timeScale_ = scale;
}

TimeState TimeRegion::createSnapshot() const {
    return TimeState(localTime_);
}

void TimeRegion::restoreSnapshot(const TimeState& state) {
    localTime_ = state.getTimestamp();
}

Timeline::Timeline()
    : currentTime_(0.0),
      globalTimeScale_(1.0),
      isPaused_(false),
      automaticSnapshots_(false),
      snapshotInterval_(1.0),
      snapshotCounter_(0.0) {}

void Timeline::update(double deltaTime) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (isPaused_) {
        return;
    }

    double scaledDelta = deltaTime * globalTimeScale_;
    currentTime_ += scaledDelta;

    if (automaticSnapshots_) {
        snapshotCounter_ += deltaTime;
        while (snapshotCounter_ >= snapshotInterval_) {
            history_.push_back(createSnapshotLocked());
            snapshotCounter_ -= snapshotInterval_;

            if (history_.size() > K_MAX_HISTORY_SIZE) {
                history_.pop_front();
            }
        }
    }

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
                           [region](const std::unique_ptr<TimeRegion>& r) { return r.get() == region; });

    if (it != regions_.end()) {
        regions_.erase(it);
    }
}

TimeState Timeline::createSnapshot() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return createSnapshotLocked();
}

TimeState Timeline::createSnapshotLocked() const {
    TimeState state(currentTime_);
    for (auto* provider : providers_) {
        int64_t token = provider->onCreateSnapshot(currentTime_);
        std::string key = std::string("__provider:") + provider->providerName();
        state.setEntityState(key, token);
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
    for (auto* provider : providers_) {
        std::string key = std::string("__provider:") + provider->providerName();
        auto token = state.getEntityState<int64_t>(key);
        if (token.has_value()) {
            provider->onRestoreSnapshot(token.value());
        }
    }
}

void Timeline::addProvider(SnapshotProvider* provider) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = std::find(providers_.begin(), providers_.end(), provider);
    if (it == providers_.end()) {
        providers_.push_back(provider);
    }
}

void Timeline::removeProvider(SnapshotProvider* provider) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = std::find(providers_.begin(), providers_.end(), provider);
    if (it != providers_.end()) {
        providers_.erase(it);
    }
}

double Timeline::getCurrentTime() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return currentTime_;
}

void Timeline::setGlobalTimeScale(double scale) {
    std::lock_guard<std::mutex> lock(mutex_);
    globalTimeScale_ = scale;
}

double Timeline::getGlobalTimeScale() const {
    std::lock_guard<std::mutex> lock(mutex_);
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
    std::lock_guard<std::mutex> lock(mutex_);
    return isPaused_;
}

void Timeline::setAutomaticSnapshots(bool enable, double interval) {
    std::lock_guard<std::mutex> lock(mutex_);
    automaticSnapshots_ = enable;
    snapshotInterval_ = interval;
    snapshotCounter_ = 0.0;
}

const std::deque<TimeState>& Timeline::getHistory() const {
    std::lock_guard<std::mutex> lock(mutex_);
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

} // namespace fabric
