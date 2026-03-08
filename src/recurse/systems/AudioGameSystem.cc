#include "recurse/systems/AudioGameSystem.hh"

#include "fabric/core/AppContext.hh"
#include "fabric/core/Log.hh"
#include "fabric/core/SystemRegistry.hh"
#include "fabric/utils/Profiler.hh"
#include "recurse/systems/CameraGameSystem.hh"
#include "recurse/systems/TerrainSystem.hh"
#include "recurse/systems/VoxelSimulationSystem.hh"

namespace recurse::systems {

void AudioGameSystem::doInit(fabric::AppContext& ctx) {
    camera_ = ctx.systemRegistry.get<CameraGameSystem>();
    terrain_ = ctx.systemRegistry.get<TerrainSystem>();
    voxelSim_ = ctx.systemRegistry.get<VoxelSimulationSystem>();

    audioSystem_.setThreadedMode(true);
    audioSystem_.init();
    audioSystem_.setSimulationGrid(&voxelSim_->simulationGrid());

    FABRIC_LOG_INFO("AudioGameSystem initialized");
}

void AudioGameSystem::update(fabric::AppContext& /*ctx*/, float dt) {
    FABRIC_ZONE_SCOPED_N("audio_update");

    audioSystem_.setListenerPosition(camera_->position());
    audioSystem_.setListenerDirection(camera_->forward(), camera_->up());
    audioSystem_.update(dt);
}

void AudioGameSystem::doShutdown() {
    audioSystem_.shutdown();
    voxelSim_ = nullptr;
    FABRIC_LOG_INFO("AudioGameSystem shut down");
}

void AudioGameSystem::configureDependencies() {
    after<CameraGameSystem>();
    after<TerrainSystem>();
    after<VoxelSimulationSystem>();
}

} // namespace recurse::systems
