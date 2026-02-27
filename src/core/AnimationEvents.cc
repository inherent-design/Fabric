#include "fabric/core/AnimationEvents.hh"

#include <algorithm>

namespace fabric {

void AnimationEvents::init() {}

void AnimationEvents::shutdown() {
    clips_.clear();
    callback_ = nullptr;
}

ClipId AnimationEvents::registerClip(const std::string& name) {
    ClipId id = nextClipId_++;
    clips_[id] = ClipData{name, {}};
    return id;
}

void AnimationEvents::addMarker(ClipId clip, const AnimEventMarker& marker) {
    auto it = clips_.find(clip);
    if (it == clips_.end())
        return;

    auto& markers = it->second.markers;
    auto pos = std::lower_bound(markers.begin(), markers.end(), marker,
                                [](const AnimEventMarker& a, const AnimEventMarker& b) { return a.time < b.time; });
    markers.insert(pos, marker);
}

void AnimationEvents::clearMarkers(ClipId clip) {
    auto it = clips_.find(clip);
    if (it != clips_.end()) {
        it->second.markers.clear();
    }
}

void AnimationEvents::removeClip(ClipId clip) {
    clips_.erase(clip);
}

std::vector<AnimEventData> AnimationEvents::processEvents(ClipId clip, float prevTime, float currTime) {
    std::vector<AnimEventData> result;

    auto it = clips_.find(clip);
    if (it == clips_.end())
        return result;

    const auto& markers = it->second.markers;
    if (markers.empty())
        return result;

    auto emit = [&](const AnimEventMarker& m) {
        AnimEventData data{m.type, m.soundPath, m.volume, m.tag, m.time};
        if (callback_)
            callback_(data);
        result.push_back(std::move(data));
    };

    if (currTime >= prevTime) {
        // Forward playback: fire markers in (prevTime, currTime]
        AnimEventMarker key{prevTime, {}, {}, 0.0f, {}};
        auto start =
            std::upper_bound(markers.begin(), markers.end(), key,
                             [](const AnimEventMarker& a, const AnimEventMarker& b) { return a.time < b.time; });
        for (auto mi = start; mi != markers.end() && mi->time <= currTime; ++mi) {
            emit(*mi);
        }
    } else {
        // Wrap-around: animation looped. Fire (prevTime, 1.0] then [0.0, currTime]
        AnimEventMarker key{prevTime, {}, {}, 0.0f, {}};
        auto start =
            std::upper_bound(markers.begin(), markers.end(), key,
                             [](const AnimEventMarker& a, const AnimEventMarker& b) { return a.time < b.time; });
        for (auto mi = start; mi != markers.end(); ++mi) {
            emit(*mi);
        }
        for (auto mi = markers.begin(); mi != markers.end() && mi->time <= currTime; ++mi) {
            emit(*mi);
        }
    }

    return result;
}

void AnimationEvents::setEventCallback(AnimEventCallback cb) {
    callback_ = std::move(cb);
}

uint32_t AnimationEvents::clipCount() const {
    return static_cast<uint32_t>(clips_.size());
}

uint32_t AnimationEvents::markerCount(ClipId clip) const {
    auto it = clips_.find(clip);
    if (it == clips_.end())
        return 0;
    return static_cast<uint32_t>(it->second.markers.size());
}

const std::string& AnimationEvents::clipName(ClipId clip) const {
    static const std::string empty;
    auto it = clips_.find(clip);
    if (it == clips_.end())
        return empty;
    return it->second.name;
}

} // namespace fabric
