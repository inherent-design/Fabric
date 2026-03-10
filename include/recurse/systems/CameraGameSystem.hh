#pragma once

#include "fabric/core/SystemBase.hh"
#include "fabric/render/Rendering.hh"
#include "recurse/character/CameraController.hh"

#include <memory>

namespace recurse::systems {

class CharacterMovementSystem;
class TerrainSystem;
class VoxelSimulationSystem;

/// Camera controller wrapper. Processes mouse input once per frame,
/// then tracks the player position with spring arm collision.
class CameraGameSystem : public fabric::System<CameraGameSystem> {
  public:
    void doInit(fabric::AppContext& ctx) override;
    void update(fabric::AppContext& ctx, float dt) override;
    void doShutdown() override;
    void configureDependencies() override;

    fabric::Vector3<float, fabric::Space::World> position() const;
    fabric::Vector3<float, fabric::Space::World> forward() const;
    fabric::Vector3<float, fabric::Space::World> right() const;
    fabric::Vector3<float, fabric::Space::World> up() const;

    recurse::CameraController& cameraController() { return *cameraCtrl_; }

  private:
    std::unique_ptr<recurse::CameraController> cameraCtrl_;

    CharacterMovementSystem* charMovement_ = nullptr;
    TerrainSystem* terrain_ = nullptr;
    VoxelSimulationSystem* voxelSim_ = nullptr;
};

} // namespace recurse::systems
