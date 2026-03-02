#pragma once

#include "fabric/core/SystemBase.hh"
#include <memory>

namespace recurse {
class VoxelInteraction;
} // namespace recurse

namespace recurse::systems {

class TerrainSystem;
class CameraGameSystem;

/// Handles mouse-button voxel editing (destroy/create matter).
/// Raycasts from camera position along camera forward, with a
/// cooldown to rate-limit interactions.
class VoxelInteractionSystem : public fabric::System<VoxelInteractionSystem> {
  public:
    VoxelInteractionSystem() = default;

    void init(fabric::AppContext& ctx) override;
    void fixedUpdate(fabric::AppContext& ctx, float fixedDt) override;

    void configureDependencies() override;

  private:
    static constexpr float kInteractionRate = 0.15f;

    TerrainSystem* terrain_ = nullptr;
    CameraGameSystem* camera_ = nullptr;

    std::unique_ptr<VoxelInteraction> voxelInteraction_;
    float interactionCooldown_ = 0.0f;
};

} // namespace recurse::systems
