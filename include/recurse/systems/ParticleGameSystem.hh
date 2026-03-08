#pragma once

#include "fabric/core/SystemBase.hh"
#include "recurse/render/ParticleSystem.hh"
#include "recurse/simulation/DebrisPool.hh"

namespace recurse::systems {

/// Manages particle simulation and debris-to-particle conversion.
/// Runs during FixedUpdate. Exposes renderParticles() for the Render phase
/// caller (VoxelRenderSystem) since particle billboard submission requires
/// camera matrices available only at render time.
class ParticleGameSystem : public fabric::System<ParticleGameSystem> {
  public:
    void doInit(fabric::AppContext& ctx) override;
    void fixedUpdate(fabric::AppContext& ctx, float fixedDt) override;
    void doShutdown() override;
    void configureDependencies() override;

    /// Called by the Render phase to submit particle billboard draws.
    void renderParticles(const float* viewMtx, const float* projMtx, uint16_t width, uint16_t height);

    recurse::ParticleSystem& particleSystem() { return particleSystem_; }
    recurse::DebrisPool& debrisPool() { return debrisPool_; }

  private:
    recurse::ParticleSystem particleSystem_;
    recurse::DebrisPool debrisPool_;
};

} // namespace recurse::systems
