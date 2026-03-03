#include "recurse/systems/ParticleGameSystem.hh"

#include "fabric/core/AppContext.hh"
#include "fabric/core/Log.hh"
#include "fabric/utils/Profiler.hh"

namespace recurse::systems {

void ParticleGameSystem::init(fabric::AppContext& /*ctx*/) {
    particleSystem_.init();

    debrisPool_.enableParticleConversion(true);
    debrisPool_.setParticleEmitter(
        [this](const fabric::Vector3<float, fabric::Space::World>& pos, float radius, int count) {
            particleSystem_.emit(pos, radius, count, recurse::ParticleType::DebrisPuff);
        });

    FABRIC_LOG_INFO("ParticleGameSystem initialized");
}

void ParticleGameSystem::fixedUpdate(fabric::AppContext& /*ctx*/, float fixedDt) {
    FABRIC_ZONE_SCOPED_N("particle_game");
    debrisPool_.update(fixedDt);
    particleSystem_.update(fixedDt);
}

void ParticleGameSystem::shutdown() {
    particleSystem_.shutdown();
    FABRIC_LOG_INFO("ParticleGameSystem shut down");
}

void ParticleGameSystem::configureDependencies() {
    // No dependencies; particles are self-contained
}

void ParticleGameSystem::renderParticles(const float* viewMtx, const float* projMtx, uint16_t width, uint16_t height) {
    particleSystem_.render(viewMtx, projMtx, width, height);
}

} // namespace recurse::systems
