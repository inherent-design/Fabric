#pragma once

#include "fabric/core/ChunkedGrid.hh"
#include "fabric/core/Spatial.hh"
#include <cstdint>
#include <string>
#include <unordered_map>

struct ma_engine;
struct ma_sound;

namespace fabric {

using SoundHandle = uint32_t;
constexpr SoundHandle InvalidSoundHandle = 0;

using Vec3f = Vector3<float, Space::World>;

enum class AttenuationModel : uint8_t {
    Inverse,
    Linear,
    Exponential
};

struct OcclusionResult {
    float factor;
    int solidCount;
    int totalSteps;
};

class AudioSystem {
  public:
    AudioSystem();
    ~AudioSystem();

    AudioSystem(const AudioSystem&) = delete;
    AudioSystem& operator=(const AudioSystem&) = delete;

    // Lifecycle
    void init();
    void initHeadless();
    void shutdown();
    void update(float dt);

    bool isInitialized() const;

    // Listener
    void setListenerPosition(const Vec3f& pos);
    void setListenerDirection(const Vec3f& forward, const Vec3f& up);

    // Sound playback
    SoundHandle playSound(const std::string& path, const Vec3f& position);
    SoundHandle playSoundLooped(const std::string& path, const Vec3f& position);
    void stopSound(SoundHandle handle);
    void stopAllSounds();

    // Sound manipulation
    void setSoundPosition(SoundHandle handle, const Vec3f& pos);
    void setSoundVolume(SoundHandle handle, float volume);
    bool isSoundPlaying(SoundHandle handle) const;

    // Configuration
    void setMasterVolume(float volume);
    void setAttenuationModel(AttenuationModel model);

    // Occlusion
    void setDensityGrid(const ChunkedGrid<float>* grid);
    OcclusionResult computeOcclusion(const Vec3f& source, const Vec3f& listener, float threshold = 0.5f) const;
    void setOcclusionEnabled(bool enabled);
    bool isOcclusionEnabled() const;

    // Stats
    uint32_t activeSoundCount() const;

  private:
    SoundHandle nextHandle();
    // TODO(human): Implement the sound cleanup strategy
    void cleanupFinishedSounds();

    ma_engine* engine_ = nullptr;
    bool initialized_ = false;
    SoundHandle handleCounter_ = 0;
    std::unordered_map<SoundHandle, ma_sound*> activeSounds_;
    float masterVolume_ = 1.0f;

    const ChunkedGrid<float>* densityGrid_ = nullptr;
    bool occlusionEnabled_ = false;
    std::unordered_map<SoundHandle, float> baseVolumes_;
    std::unordered_map<SoundHandle, Vec3f> soundPositions_;
    Vec3f listenerPos_{};
};

} // namespace fabric
