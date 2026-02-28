#pragma once

#include "fabric/core/ChunkedGrid.hh"
#include "fabric/core/Spatial.hh"
#include <array>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
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

// SPSC command types for game-to-audio decoupling
enum class AudioCommandType : uint8_t {
    Play,
    PlayLooped,
    Stop,
    StopAll,
    SetPosition,
    SetVolume,
    SetListenerPosition,
    SetListenerDirection
};

enum class SoundCategory : uint8_t {
    Master = 0,
    SFX,
    Music,
    Ambient,
    UI,
    Count
};

struct AudioCommand {
    AudioCommandType type;
    SoundHandle handle = InvalidSoundHandle;
    std::string path;
    Vec3f position;
    Vec3f direction;
    Vec3f up;
    float volume = 1.0f;
    SoundCategory category = SoundCategory::SFX;
};

// Lock-free single-producer single-consumer ring buffer.
// Producer writes head_, consumer writes tail_. N must be power of 2.
template <typename T, size_t N> class SPSCRingBuffer {
    static_assert((N & (N - 1)) == 0, "N must be a power of 2");

  public:
    bool tryPush(T&& item) {
        size_t h = head_.load(std::memory_order_relaxed);
        size_t t = tail_.load(std::memory_order_acquire);
        if (h - t >= N)
            return false;
        buffer_[h & (N - 1)] = std::move(item);
        head_.store(h + 1, std::memory_order_release);
        return true;
    }

    bool tryPop(T& item) {
        size_t t = tail_.load(std::memory_order_relaxed);
        size_t h = head_.load(std::memory_order_acquire);
        if (t >= h)
            return false;
        item = std::move(buffer_[t & (N - 1)]);
        tail_.store(t + 1, std::memory_order_release);
        return true;
    }

    size_t size() const {
        size_t h = head_.load(std::memory_order_acquire);
        size_t t = tail_.load(std::memory_order_acquire);
        return h - t;
    }

  private:
    std::array<T, N> buffer_;
    std::atomic<size_t> head_{0};
    std::atomic<size_t> tail_{0};
};

class AudioSystem {
  public:
    static constexpr size_t kCommandBufferSize = 256;

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

    // Command buffer control
    void setCommandBufferEnabled(bool enabled);
    bool isCommandBufferEnabled() const;

    // Threaded mode control
    void setThreadedMode(bool enabled);
    bool isThreadedMode() const;

    // Listener
    void setListenerPosition(const Vec3f& pos);
    void setListenerDirection(const Vec3f& forward, const Vec3f& up);

    // Sound playback
    SoundHandle playSound(const std::string& path, const Vec3f& position);
    SoundHandle playSoundLooped(const std::string& path, const Vec3f& position);
    SoundHandle playSound(const std::string& path, const Vec3f& position, SoundCategory category);
    SoundHandle playSoundLooped(const std::string& path, const Vec3f& position, SoundCategory category);
    void stopSound(SoundHandle handle);
    void stopAllSounds();

    // Sound manipulation
    void setSoundPosition(SoundHandle handle, const Vec3f& pos);
    void setSoundVolume(SoundHandle handle, float volume);
    bool isSoundPlaying(SoundHandle handle) const;

    // Configuration
    void setMasterVolume(float volume);
    void setAttenuationModel(AttenuationModel model);

    // Sound categories
    void setCategoryVolume(SoundCategory category, float volume);
    float getCategoryVolume(SoundCategory category) const;

    // Occlusion
    void setDensityGrid(const ChunkedGrid<float>* grid);
    OcclusionResult computeOcclusion(const Vec3f& source, const Vec3f& listener, float threshold = 0.5f) const;
    void setOcclusionEnabled(bool enabled);
    bool isOcclusionEnabled() const;

    // Stats
    uint32_t activeSoundCount() const;

  private:
    SoundHandle nextHandle();
    void cleanupFinishedSounds();
    void drainCommandBuffer();
    void executeCommand(AudioCommand& cmd);
    void recalculateVolume(SoundHandle handle);

    SoundHandle executePlay(const std::string& path, const Vec3f& position, bool looped, SoundCategory category,
                            SoundHandle preAllocHandle);
    void executeStop(SoundHandle handle);
    void executeStopAll();
    void executeSetPosition(SoundHandle handle, const Vec3f& pos);
    void executeSetVolume(SoundHandle handle, float volume);
    void executeSetListenerPosition(const Vec3f& pos);
    void executeSetListenerDirection(const Vec3f& forward, const Vec3f& up);

    void audioThreadLoop();

    ma_engine* engine_ = nullptr;
    bool initialized_ = false;
    bool commandBufferEnabled_ = false;
    bool threadedMode_ = false;
    SoundHandle handleCounter_ = 0;

    std::unordered_map<SoundHandle, ma_sound*> activeSounds_;
    std::unordered_map<SoundHandle, float> baseVolumes_;
    std::unordered_map<SoundHandle, Vec3f> soundPositions_;
    std::unordered_map<SoundHandle, SoundCategory> soundCategories_;

    float masterVolume_ = 1.0f;
    std::array<float, static_cast<size_t>(SoundCategory::Count)> categoryVolumes_;

    Vec3f listenerPos_{};

    AttenuationModel attenuationModel_ = AttenuationModel::Inverse;

    const ChunkedGrid<float>* densityGrid_ = nullptr;
    bool occlusionEnabled_ = false;

    SPSCRingBuffer<AudioCommand, kCommandBufferSize> commandBuffer_;

    std::thread audioThread_;
    std::atomic<bool> audioThreadRunning_{false};
    mutable std::mutex soundsMutex_;
    std::mutex listenerMutex_;
};

} // namespace fabric
