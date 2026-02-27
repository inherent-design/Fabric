#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>

#include "fabric/core/AudioSystem.hh"
#include "fabric/core/Log.hh"
#include "fabric/core/VoxelRaycast.hh"
#include "fabric/utils/ErrorHandling.hh"

#include <algorithm>
#include <cmath>

namespace fabric {

namespace {

ma_attenuation_model toMaModel(AttenuationModel model) {
    switch (model) {
        case AttenuationModel::Inverse:
            return ma_attenuation_model_inverse;
        case AttenuationModel::Linear:
            return ma_attenuation_model_linear;
        case AttenuationModel::Exponential:
            return ma_attenuation_model_exponential;
    }
    return ma_attenuation_model_inverse;
}

} // namespace

AudioSystem::AudioSystem() = default;

AudioSystem::~AudioSystem() {
    if (initialized_) {
        shutdown();
    }
}

void AudioSystem::init() {
    if (initialized_) {
        FABRIC_LOG_WARN("AudioSystem already initialized");
        return;
    }

    engine_ = new ma_engine;

    ma_engine_config config = ma_engine_config_init();
    config.listenerCount = 1;

    ma_result result = ma_engine_init(&config, engine_);
    if (result != MA_SUCCESS) {
        delete engine_;
        engine_ = nullptr;
        throwError("Failed to initialize miniaudio engine: " + std::to_string(result));
    }

    initialized_ = true;
    FABRIC_LOG_INFO("AudioSystem initialized with device audio");
}

void AudioSystem::initHeadless() {
    if (initialized_) {
        FABRIC_LOG_WARN("AudioSystem already initialized");
        return;
    }

    engine_ = new ma_engine;

    ma_engine_config config = ma_engine_config_init();
    config.listenerCount = 1;
    config.noDevice = MA_TRUE;
    config.channels = 2;
    config.sampleRate = 48000;

    ma_result result = ma_engine_init(&config, engine_);
    if (result != MA_SUCCESS) {
        delete engine_;
        engine_ = nullptr;
        throwError("Failed to initialize miniaudio engine (headless): " + std::to_string(result));
    }

    initialized_ = true;
    FABRIC_LOG_INFO("AudioSystem initialized in headless mode");
}

void AudioSystem::shutdown() {
    if (!initialized_) {
        return;
    }

    stopAllSounds();
    ma_engine_uninit(engine_);
    delete engine_;
    engine_ = nullptr;
    initialized_ = false;
    handleCounter_ = 0;
    soundPositions_.clear();
    baseVolumes_.clear();
    densityGrid_ = nullptr;
    occlusionEnabled_ = false;
    FABRIC_LOG_INFO("AudioSystem shut down");
}

void AudioSystem::update(float dt) {
    (void)dt;
    if (!initialized_) {
        return;
    }
    cleanupFinishedSounds();

    if (occlusionEnabled_ && densityGrid_) {
        for (auto& [handle, sound] : activeSounds_) {
            auto posIt = soundPositions_.find(handle);
            if (posIt == soundPositions_.end())
                continue;

            auto occ = computeOcclusion(posIt->second, listenerPos_);
            float baseVol = 1.0f;
            auto volIt = baseVolumes_.find(handle);
            if (volIt != baseVolumes_.end()) {
                baseVol = volIt->second;
            }
            float effective = baseVol * (1.0f - occ.factor * 0.8f);
            ma_sound_set_volume(sound, effective);
        }
    }
}

bool AudioSystem::isInitialized() const {
    return initialized_;
}

void AudioSystem::setListenerPosition(const Vec3f& pos) {
    listenerPos_ = pos;
    if (!initialized_)
        return;
    ma_engine_listener_set_position(engine_, 0, pos.x, pos.y, pos.z);
}

void AudioSystem::setListenerDirection(const Vec3f& forward, const Vec3f& up) {
    if (!initialized_)
        return;
    ma_engine_listener_set_direction(engine_, 0, forward.x, forward.y, forward.z);
    ma_engine_listener_set_world_up(engine_, 0, up.x, up.y, up.z);
}

SoundHandle AudioSystem::playSound(const std::string& path, const Vec3f& position) {
    if (!initialized_) {
        FABRIC_LOG_WARN("Cannot play sound: AudioSystem not initialized");
        return InvalidSoundHandle;
    }

    auto* sound = new ma_sound;
    ma_result result = ma_sound_init_from_file(engine_, path.c_str(), 0, nullptr, nullptr, sound);
    if (result != MA_SUCCESS) {
        delete sound;
        FABRIC_LOG_ERROR("Failed to load sound '{}': {}", path, static_cast<int>(result));
        return InvalidSoundHandle;
    }

    ma_sound_set_position(sound, position.x, position.y, position.z);
    ma_sound_set_spatialization_enabled(sound, MA_TRUE);

    SoundHandle handle = nextHandle();
    activeSounds_[handle] = sound;
    soundPositions_[handle] = position;
    baseVolumes_[handle] = 1.0f;

    result = ma_sound_start(sound);
    if (result != MA_SUCCESS) {
        ma_sound_uninit(sound);
        delete sound;
        activeSounds_.erase(handle);
        soundPositions_.erase(handle);
        baseVolumes_.erase(handle);
        FABRIC_LOG_ERROR("Failed to start sound '{}': {}", path, static_cast<int>(result));
        return InvalidSoundHandle;
    }

    FABRIC_LOG_DEBUG("Playing sound '{}' at ({}, {}, {}), handle={}", path, position.x, position.y, position.z, handle);
    return handle;
}

SoundHandle AudioSystem::playSoundLooped(const std::string& path, const Vec3f& position) {
    if (!initialized_) {
        FABRIC_LOG_WARN("Cannot play sound: AudioSystem not initialized");
        return InvalidSoundHandle;
    }

    auto* sound = new ma_sound;
    ma_result result = ma_sound_init_from_file(engine_, path.c_str(), 0, nullptr, nullptr, sound);
    if (result != MA_SUCCESS) {
        delete sound;
        FABRIC_LOG_ERROR("Failed to load sound '{}': {}", path, static_cast<int>(result));
        return InvalidSoundHandle;
    }

    ma_sound_set_position(sound, position.x, position.y, position.z);
    ma_sound_set_spatialization_enabled(sound, MA_TRUE);
    ma_sound_set_looping(sound, MA_TRUE);

    SoundHandle handle = nextHandle();
    activeSounds_[handle] = sound;
    soundPositions_[handle] = position;
    baseVolumes_[handle] = 1.0f;

    result = ma_sound_start(sound);
    if (result != MA_SUCCESS) {
        ma_sound_uninit(sound);
        delete sound;
        activeSounds_.erase(handle);
        soundPositions_.erase(handle);
        baseVolumes_.erase(handle);
        FABRIC_LOG_ERROR("Failed to start looped sound '{}': {}", path, static_cast<int>(result));
        return InvalidSoundHandle;
    }

    FABRIC_LOG_DEBUG("Playing looped sound '{}' at ({}, {}, {}), handle={}", path, position.x, position.y, position.z,
                     handle);
    return handle;
}

void AudioSystem::stopSound(SoundHandle handle) {
    auto it = activeSounds_.find(handle);
    if (it == activeSounds_.end()) {
        return;
    }

    ma_sound_stop(it->second);
    ma_sound_uninit(it->second);
    delete it->second;
    soundPositions_.erase(handle);
    baseVolumes_.erase(handle);
    activeSounds_.erase(it);
}

void AudioSystem::stopAllSounds() {
    for (auto& [handle, sound] : activeSounds_) {
        ma_sound_stop(sound);
        ma_sound_uninit(sound);
        delete sound;
    }
    activeSounds_.clear();
    soundPositions_.clear();
    baseVolumes_.clear();
}

void AudioSystem::setSoundPosition(SoundHandle handle, const Vec3f& pos) {
    auto it = activeSounds_.find(handle);
    if (it == activeSounds_.end())
        return;
    ma_sound_set_position(it->second, pos.x, pos.y, pos.z);
    soundPositions_[handle] = pos;
}

void AudioSystem::setSoundVolume(SoundHandle handle, float volume) {
    auto it = activeSounds_.find(handle);
    if (it == activeSounds_.end())
        return;
    baseVolumes_[handle] = volume;
    ma_sound_set_volume(it->second, volume);
}

bool AudioSystem::isSoundPlaying(SoundHandle handle) const {
    auto it = activeSounds_.find(handle);
    if (it == activeSounds_.end())
        return false;
    return ma_sound_is_playing(it->second) != 0;
}

void AudioSystem::setMasterVolume(float volume) {
    masterVolume_ = volume;
    if (initialized_) {
        ma_engine_set_volume(engine_, volume);
    }
}

void AudioSystem::setAttenuationModel(AttenuationModel model) {
    (void)model;
    // Attenuation model is set per-sound in miniaudio.
    // This stores the default for future sounds.
    // Per-sound override can be added if needed.
    FABRIC_LOG_DEBUG("Attenuation model set to {}", static_cast<int>(model));
}

uint32_t AudioSystem::activeSoundCount() const {
    return static_cast<uint32_t>(activeSounds_.size());
}

SoundHandle AudioSystem::nextHandle() {
    ++handleCounter_;
    if (handleCounter_ == InvalidSoundHandle) {
        ++handleCounter_;
    }
    return handleCounter_;
}

void AudioSystem::setDensityGrid(const ChunkedGrid<float>* grid) {
    densityGrid_ = grid;
}

OcclusionResult AudioSystem::computeOcclusion(const Vec3f& source, const Vec3f& listener, float threshold) const {
    if (!densityGrid_) {
        return {0.0f, 0, 0};
    }

    float dx = listener.x - source.x;
    float dy = listener.y - source.y;
    float dz = listener.z - source.z;
    float distance = std::sqrt(dx * dx + dy * dy + dz * dz);

    if (distance < 1e-6f) {
        return {0.0f, 0, 0};
    }

    // Direction is unnormalized (source-to-listener). With t in [0,1],
    // t=1.0 reaches the listener exactly, so maxDistance=1.0 traces
    // the full source-to-listener segment.
    auto hits = castRayAll(*densityGrid_, source.x, source.y, source.z, dx, dy, dz, 1.0f, threshold);

    int solidCount = static_cast<int>(hits.size());
    int totalSteps = static_cast<int>(std::ceil(distance));

    constexpr int kMaxOcclusionVoxels = 8;
    float factor = std::min(static_cast<float>(solidCount) / static_cast<float>(kMaxOcclusionVoxels), 1.0f);

    return {factor, solidCount, totalSteps};
}

void AudioSystem::setOcclusionEnabled(bool enabled) {
    occlusionEnabled_ = enabled;
}

bool AudioSystem::isOcclusionEnabled() const {
    return occlusionEnabled_;
}

void AudioSystem::cleanupFinishedSounds() {
    std::vector<SoundHandle> finished;
    for (auto& [handle, sound] : activeSounds_) {
        if (ma_sound_at_end(sound) && !ma_sound_is_looping(sound)) {
            finished.push_back(handle);
        }
    }
    for (auto handle : finished) {
        auto it = activeSounds_.find(handle);
        if (it != activeSounds_.end()) {
            ma_sound_uninit(it->second);
            delete it->second;
            activeSounds_.erase(it);
            soundPositions_.erase(handle);
            baseVolumes_.erase(handle);
        }
    }
}

} // namespace fabric
