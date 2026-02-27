#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>

#include "fabric/core/AudioSystem.hh"
#include "fabric/core/Log.hh"
#include "fabric/core/VoxelRaycast.hh"
#include "fabric/utils/ErrorHandling.hh"

#include <algorithm>
#include <cmath>
#include <filesystem>

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

AudioSystem::AudioSystem() {
    categoryVolumes_.fill(1.0f);
}

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

    commandBufferEnabled_ = true;
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

    commandBufferEnabled_ = true;
    initialized_ = true;
    FABRIC_LOG_INFO("AudioSystem initialized in headless mode");
}

void AudioSystem::shutdown() {
    if (!initialized_) {
        return;
    }

    drainCommandBuffer();
    executeStopAll();

    ma_engine_uninit(engine_);
    delete engine_;
    engine_ = nullptr;
    initialized_ = false;
    commandBufferEnabled_ = false;
    handleCounter_ = 0;
    soundPositions_.clear();
    baseVolumes_.clear();
    soundCategories_.clear();
    densityGrid_ = nullptr;
    occlusionEnabled_ = false;
    attenuationModel_ = AttenuationModel::Inverse;
    categoryVolumes_.fill(1.0f);
    masterVolume_ = 1.0f;
    FABRIC_LOG_INFO("AudioSystem shut down");
}

void AudioSystem::update(float dt) {
    (void)dt;
    if (!initialized_) {
        return;
    }

    drainCommandBuffer();
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

            float catVol = 1.0f;
            auto catIt = soundCategories_.find(handle);
            if (catIt != soundCategories_.end()) {
                catVol = categoryVolumes_[static_cast<size_t>(catIt->second)];
            }

            float effective = catVol * baseVol * (1.0f - occ.factor * 0.8f);
            ma_sound_set_volume(sound, effective);
        }
    }
}

bool AudioSystem::isInitialized() const {
    return initialized_;
}

void AudioSystem::setCommandBufferEnabled(bool enabled) {
    commandBufferEnabled_ = enabled;
}

bool AudioSystem::isCommandBufferEnabled() const {
    return commandBufferEnabled_;
}

// --- Listener ---

void AudioSystem::setListenerPosition(const Vec3f& pos) {
    listenerPos_ = pos;
    if (!initialized_)
        return;
    if (commandBufferEnabled_) {
        AudioCommand cmd;
        cmd.type = AudioCommandType::SetListenerPosition;
        cmd.position = pos;
        if (!commandBuffer_.tryPush(std::move(cmd))) {
            FABRIC_LOG_WARN("Audio command buffer full, dropping SetListenerPosition");
        }
        return;
    }
    ma_engine_listener_set_position(engine_, 0, pos.x, pos.y, pos.z);
}

void AudioSystem::setListenerDirection(const Vec3f& forward, const Vec3f& up) {
    if (!initialized_)
        return;
    if (commandBufferEnabled_) {
        AudioCommand cmd;
        cmd.type = AudioCommandType::SetListenerDirection;
        cmd.direction = forward;
        cmd.up = up;
        if (!commandBuffer_.tryPush(std::move(cmd))) {
            FABRIC_LOG_WARN("Audio command buffer full, dropping SetListenerDirection");
        }
        return;
    }
    ma_engine_listener_set_direction(engine_, 0, forward.x, forward.y, forward.z);
    ma_engine_listener_set_world_up(engine_, 0, up.x, up.y, up.z);
}

// --- Sound playback ---

SoundHandle AudioSystem::playSound(const std::string& path, const Vec3f& position) {
    return playSound(path, position, SoundCategory::SFX);
}

SoundHandle AudioSystem::playSoundLooped(const std::string& path, const Vec3f& position) {
    return playSoundLooped(path, position, SoundCategory::SFX);
}

SoundHandle AudioSystem::playSound(const std::string& path, const Vec3f& position, SoundCategory category) {
    if (!initialized_) {
        FABRIC_LOG_WARN("Cannot play sound: AudioSystem not initialized");
        return InvalidSoundHandle;
    }

    if (commandBufferEnabled_) {
        SoundHandle handle = nextHandle();
        AudioCommand cmd;
        cmd.type = AudioCommandType::Play;
        cmd.handle = handle;
        cmd.path = path;
        cmd.position = position;
        cmd.category = category;
        if (!commandBuffer_.tryPush(std::move(cmd))) {
            FABRIC_LOG_WARN("Audio command buffer full, dropping Play command");
            return InvalidSoundHandle;
        }
        return handle;
    }

    return executePlay(path, position, false, category, nextHandle());
}

SoundHandle AudioSystem::playSoundLooped(const std::string& path, const Vec3f& position, SoundCategory category) {
    if (!initialized_) {
        FABRIC_LOG_WARN("Cannot play sound: AudioSystem not initialized");
        return InvalidSoundHandle;
    }

    if (commandBufferEnabled_) {
        SoundHandle handle = nextHandle();
        AudioCommand cmd;
        cmd.type = AudioCommandType::PlayLooped;
        cmd.handle = handle;
        cmd.path = path;
        cmd.position = position;
        cmd.category = category;
        if (!commandBuffer_.tryPush(std::move(cmd))) {
            FABRIC_LOG_WARN("Audio command buffer full, dropping PlayLooped command");
            return InvalidSoundHandle;
        }
        return handle;
    }

    return executePlay(path, position, true, category, nextHandle());
}

void AudioSystem::stopSound(SoundHandle handle) {
    if (commandBufferEnabled_) {
        AudioCommand cmd;
        cmd.type = AudioCommandType::Stop;
        cmd.handle = handle;
        if (!commandBuffer_.tryPush(std::move(cmd))) {
            FABRIC_LOG_WARN("Audio command buffer full, dropping Stop command");
        }
        return;
    }
    executeStop(handle);
}

void AudioSystem::stopAllSounds() {
    if (commandBufferEnabled_) {
        AudioCommand cmd;
        cmd.type = AudioCommandType::StopAll;
        if (!commandBuffer_.tryPush(std::move(cmd))) {
            FABRIC_LOG_WARN("Audio command buffer full, dropping StopAll command");
        }
        return;
    }
    executeStopAll();
}

// --- Sound manipulation ---

void AudioSystem::setSoundPosition(SoundHandle handle, const Vec3f& pos) {
    if (commandBufferEnabled_) {
        AudioCommand cmd;
        cmd.type = AudioCommandType::SetPosition;
        cmd.handle = handle;
        cmd.position = pos;
        if (!commandBuffer_.tryPush(std::move(cmd))) {
            FABRIC_LOG_WARN("Audio command buffer full, dropping SetPosition");
        }
        return;
    }
    executeSetPosition(handle, pos);
}

void AudioSystem::setSoundVolume(SoundHandle handle, float volume) {
    if (commandBufferEnabled_) {
        AudioCommand cmd;
        cmd.type = AudioCommandType::SetVolume;
        cmd.handle = handle;
        cmd.volume = volume;
        if (!commandBuffer_.tryPush(std::move(cmd))) {
            FABRIC_LOG_WARN("Audio command buffer full, dropping SetVolume");
        }
        return;
    }
    executeSetVolume(handle, volume);
}

bool AudioSystem::isSoundPlaying(SoundHandle handle) const {
    auto it = activeSounds_.find(handle);
    if (it == activeSounds_.end())
        return false;
    return ma_sound_is_playing(it->second) != 0;
}

// --- Configuration ---

void AudioSystem::setMasterVolume(float volume) {
    masterVolume_ = volume;
    if (initialized_) {
        ma_engine_set_volume(engine_, volume);
    }
}

void AudioSystem::setAttenuationModel(AttenuationModel model) {
    attenuationModel_ = model;
    if (initialized_) {
        ma_attenuation_model maModel = toMaModel(model);
        for (auto& [handle, sound] : activeSounds_) {
            ma_sound_set_attenuation_model(sound, maModel);
        }
    }
    FABRIC_LOG_DEBUG("Attenuation model set to {}", static_cast<int>(model));
}

// --- Sound categories ---

void AudioSystem::setCategoryVolume(SoundCategory category, float volume) {
    if (category >= SoundCategory::Count)
        return;

    if (category == SoundCategory::Master) {
        setMasterVolume(volume);
        return;
    }

    categoryVolumes_[static_cast<size_t>(category)] = volume;
    for (auto& [handle, cat] : soundCategories_) {
        if (cat == category) {
            recalculateVolume(handle);
        }
    }
}

float AudioSystem::getCategoryVolume(SoundCategory category) const {
    if (category == SoundCategory::Master) {
        return masterVolume_;
    }
    if (category >= SoundCategory::Count)
        return 0.0f;
    return categoryVolumes_[static_cast<size_t>(category)];
}

// --- Occlusion ---

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

// --- Stats ---

uint32_t AudioSystem::activeSoundCount() const {
    return static_cast<uint32_t>(activeSounds_.size());
}

// --- Private ---

SoundHandle AudioSystem::nextHandle() {
    ++handleCounter_;
    if (handleCounter_ == InvalidSoundHandle) {
        ++handleCounter_;
    }
    return handleCounter_;
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
            soundCategories_.erase(handle);
        }
    }
}

void AudioSystem::drainCommandBuffer() {
    AudioCommand cmd;
    while (commandBuffer_.tryPop(cmd)) {
        executeCommand(cmd);
    }
}

void AudioSystem::executeCommand(AudioCommand& cmd) {
    switch (cmd.type) {
        case AudioCommandType::Play:
            executePlay(cmd.path, cmd.position, false, cmd.category, cmd.handle);
            break;
        case AudioCommandType::PlayLooped:
            executePlay(cmd.path, cmd.position, true, cmd.category, cmd.handle);
            break;
        case AudioCommandType::Stop:
            executeStop(cmd.handle);
            break;
        case AudioCommandType::StopAll:
            executeStopAll();
            break;
        case AudioCommandType::SetPosition:
            executeSetPosition(cmd.handle, cmd.position);
            break;
        case AudioCommandType::SetVolume:
            executeSetVolume(cmd.handle, cmd.volume);
            break;
        case AudioCommandType::SetListenerPosition:
            executeSetListenerPosition(cmd.position);
            break;
        case AudioCommandType::SetListenerDirection:
            executeSetListenerDirection(cmd.direction, cmd.up);
            break;
    }
}

void AudioSystem::recalculateVolume(SoundHandle handle) {
    auto it = activeSounds_.find(handle);
    if (it == activeSounds_.end())
        return;

    float base = 1.0f;
    auto volIt = baseVolumes_.find(handle);
    if (volIt != baseVolumes_.end())
        base = volIt->second;

    float catVol = 1.0f;
    auto catIt = soundCategories_.find(handle);
    if (catIt != soundCategories_.end()) {
        catVol = categoryVolumes_[static_cast<size_t>(catIt->second)];
    }

    // Master volume handled at engine level via ma_engine_set_volume
    ma_sound_set_volume(it->second, catVol * base);
}

SoundHandle AudioSystem::executePlay(const std::string& path, const Vec3f& position, bool looped,
                                     SoundCategory category, SoundHandle preAllocHandle) {
    // Pre-validate: miniaudio's resource manager has a use-after-free on
    // missing files in headless mode (v0.11.22). Check existence first.
    if (!std::filesystem::exists(path)) {
        FABRIC_LOG_ERROR("Sound file not found: '{}'", path);
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
    ma_sound_set_attenuation_model(sound, toMaModel(attenuationModel_));
    if (looped) {
        ma_sound_set_looping(sound, MA_TRUE);
    }

    activeSounds_[preAllocHandle] = sound;
    soundPositions_[preAllocHandle] = position;
    baseVolumes_[preAllocHandle] = 1.0f;
    soundCategories_[preAllocHandle] = category;

    float catVol = categoryVolumes_[static_cast<size_t>(category)];
    ma_sound_set_volume(sound, catVol);

    result = ma_sound_start(sound);
    if (result != MA_SUCCESS) {
        ma_sound_uninit(sound);
        delete sound;
        activeSounds_.erase(preAllocHandle);
        soundPositions_.erase(preAllocHandle);
        baseVolumes_.erase(preAllocHandle);
        soundCategories_.erase(preAllocHandle);
        FABRIC_LOG_ERROR("Failed to start sound '{}': {}", path, static_cast<int>(result));
        return InvalidSoundHandle;
    }

    FABRIC_LOG_DEBUG("Playing{} sound '{}' at ({}, {}, {}), handle={}, category={}", looped ? " looped" : "", path,
                     position.x, position.y, position.z, preAllocHandle, static_cast<int>(category));
    return preAllocHandle;
}

void AudioSystem::executeStop(SoundHandle handle) {
    auto it = activeSounds_.find(handle);
    if (it == activeSounds_.end())
        return;

    ma_sound_stop(it->second);
    ma_sound_uninit(it->second);
    delete it->second;
    soundPositions_.erase(handle);
    baseVolumes_.erase(handle);
    soundCategories_.erase(handle);
    activeSounds_.erase(it);
}

void AudioSystem::executeStopAll() {
    for (auto& [handle, sound] : activeSounds_) {
        ma_sound_stop(sound);
        ma_sound_uninit(sound);
        delete sound;
    }
    activeSounds_.clear();
    soundPositions_.clear();
    baseVolumes_.clear();
    soundCategories_.clear();
}

void AudioSystem::executeSetPosition(SoundHandle handle, const Vec3f& pos) {
    auto it = activeSounds_.find(handle);
    if (it == activeSounds_.end())
        return;
    ma_sound_set_position(it->second, pos.x, pos.y, pos.z);
    soundPositions_[handle] = pos;
}

void AudioSystem::executeSetVolume(SoundHandle handle, float volume) {
    auto it = activeSounds_.find(handle);
    if (it == activeSounds_.end())
        return;
    baseVolumes_[handle] = volume;
    recalculateVolume(handle);
}

void AudioSystem::executeSetListenerPosition(const Vec3f& pos) {
    listenerPos_ = pos;
    if (!initialized_)
        return;
    ma_engine_listener_set_position(engine_, 0, pos.x, pos.y, pos.z);
}

void AudioSystem::executeSetListenerDirection(const Vec3f& forward, const Vec3f& up) {
    if (!initialized_)
        return;
    ma_engine_listener_set_direction(engine_, 0, forward.x, forward.y, forward.z);
    ma_engine_listener_set_world_up(engine_, 0, up.x, up.y, up.z);
}

} // namespace fabric
