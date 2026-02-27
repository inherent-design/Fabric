#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace fabric {

enum class AnimEventType : uint8_t {
    Footstep,
    Impact,
    Whoosh,
    Custom
};

struct AnimEventMarker {
    float time;
    AnimEventType type;
    std::string soundPath;
    float volume = 1.0f;
    std::string tag;
};

struct AnimEventData {
    AnimEventType type;
    std::string soundPath;
    float volume;
    std::string tag;
    float triggerTime;
};

using AnimEventCallback = std::function<void(const AnimEventData&)>;

using ClipId = uint32_t;
constexpr ClipId InvalidClipId = 0;

class AnimationEvents {
  public:
    void init();
    void shutdown();

    ClipId registerClip(const std::string& name);
    void addMarker(ClipId clip, const AnimEventMarker& marker);
    void clearMarkers(ClipId clip);
    void removeClip(ClipId clip);

    std::vector<AnimEventData> processEvents(ClipId clip, float prevTime, float currTime);

    void setEventCallback(AnimEventCallback cb);

    uint32_t clipCount() const;
    uint32_t markerCount(ClipId clip) const;
    const std::string& clipName(ClipId clip) const;

  private:
    struct ClipData {
        std::string name;
        std::vector<AnimEventMarker> markers;
    };

    std::unordered_map<ClipId, ClipData> clips_;
    ClipId nextClipId_ = 1;
    AnimEventCallback callback_;
};

} // namespace fabric
