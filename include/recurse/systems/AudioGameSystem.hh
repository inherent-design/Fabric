#pragma once

#include "fabric/core/SystemBase.hh"
#include "recurse/audio/AudioSystem.hh"

namespace recurse::systems {

class CameraGameSystem;
class TerrainSystem;
class VoxelSimulationSystem;

/// Spatial audio system. Tracks the camera as the listener and updates
/// the audio engine each frame.
class AudioGameSystem : public fabric::System<AudioGameSystem> {
  public:
    void doInit(fabric::AppContext& ctx) override;
    void update(fabric::AppContext& ctx, float dt) override;
    void doShutdown() override;
    void configureDependencies() override;

    void onWorldBegin();
    void onWorldEnd();

    recurse::AudioSystem& audioSystem() { return audioSystem_; }
    uint32_t activeSoundCount() const { return audioSystem_.activeSoundCount(); }

  private:
    recurse::AudioSystem audioSystem_;

    CameraGameSystem* camera_ = nullptr;
    TerrainSystem* terrain_ = nullptr;
    VoxelSimulationSystem* voxelSim_ = nullptr;
};

} // namespace recurse::systems
