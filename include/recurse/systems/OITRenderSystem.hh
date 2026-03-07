#pragma once

#include "fabric/core/SystemBase.hh"
#include "recurse/render/OITCompositor.hh"

#include <cstdint>

namespace recurse::systems {

class VoxelRenderSystem;

/// Order-independent transparency compositor. Runs during Render phase
/// after VoxelRenderSystem to composite transparent geometry over the
/// opaque backbuffer.
class OITRenderSystem : public fabric::System<OITRenderSystem> {
  public:
    void doInit(fabric::AppContext& ctx) override;
    void render(fabric::AppContext& ctx) override;
    void doShutdown() override;
    void configureDependencies() override;

    recurse::OITCompositor& oitCompositor() { return oitCompositor_; }
    bool isValid() const { return oitCompositor_.isValid(); }

    /// Called from onResize to recreate resolution-dependent framebuffers.
    void resize(uint16_t width, uint16_t height);

  private:
    recurse::OITCompositor oitCompositor_;
};

} // namespace recurse::systems
