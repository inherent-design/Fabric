#pragma once

#include "fabric/core/SystemBase.hh"
#include "fabric/render/Geometry.hh"
#include "recurse/render/ShadowSystem.hh"

#include <memory>

namespace recurse::systems {

/// Computes cascaded shadow map splits and light-space matrices each frame.
/// Runs during PreRender so shadow data is ready before geometry submission.
class ShadowRenderSystem : public fabric::System<ShadowRenderSystem> {
  public:
    void doInit(fabric::AppContext& ctx) override;
    void doShutdown() override;
    void render(fabric::AppContext& ctx) override;
    void configureDependencies() override;

    recurse::ShadowSystem& shadowSystem() { return *shadowSystem_; }
    const fabric::Vec3f& lightDirection() const { return lightDir_; }

  private:
    std::unique_ptr<recurse::ShadowSystem> shadowSystem_;
    fabric::Vec3f lightDir_;
};

} // namespace recurse::systems
