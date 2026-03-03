#include "recurse/systems/AudioGameSystem.hh"

#include "fabric/core/AppContext.hh"
#include "fabric/core/Log.hh"
#include "fabric/core/SystemRegistry.hh"
#include "fabric/utils/Profiler.hh"
#include "recurse/systems/CameraGameSystem.hh"
#include "recurse/systems/TerrainSystem.hh"

namespace recurse::systems {

void AudioGameSystem::init(fabric::AppContext& ctx) {
    camera_ = ctx.systemRegistry.get<CameraGameSystem>();
    terrain_ = ctx.systemRegistry.get<TerrainSystem>();

    audioSystem_.setThreadedMode(true);
    audioSystem_.init();
    audioSystem_.setDensityGrid(&terrain_->densityGrid());

    FABRIC_LOG_INFO("AudioGameSystem initialized");
}

void AudioGameSystem::update(fabric::AppContext& /*ctx*/, float dt) {
    FABRIC_ZONE_SCOPED_N("audio_update");

    audioSystem_.setListenerPosition(camera_->position());
    audioSystem_.setListenerDirection(camera_->forward(), camera_->up());
    audioSystem_.update(dt);
}

void AudioGameSystem::shutdown() {
    audioSystem_.shutdown();
    FABRIC_LOG_INFO("AudioGameSystem shut down");
}

void AudioGameSystem::configureDependencies() {
    after<CameraGameSystem>();
    after<TerrainSystem>();
}

} // namespace recurse::systems
