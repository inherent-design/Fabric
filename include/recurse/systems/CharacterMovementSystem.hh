#pragma once

#include "fabric/core/SystemBase.hh"
#include "fabric/render/Geometry.hh"
#include "recurse/character/CharacterTypes.hh"
#include "recurse/character/GameConstants.hh"
#include "recurse/character/MovementFSM.hh"

#include <flecs.h>
#include <memory>

namespace recurse {
class CharacterController;
class FlightController;
class CameraController;
class JoltCharacterController;
} // namespace recurse

namespace recurse::systems {

class TerrainSystem;
class CameraGameSystem;
class PhysicsGameSystem;
class VoxelSimulationSystem;

/// Owns player movement state: controllers, FSM, position, velocity.
/// Reads camera direction for movement input and resolves character
/// collision against the density grid each fixed tick.
class CharacterMovementSystem : public fabric::System<CharacterMovementSystem> {
  public:
    CharacterMovementSystem() = default;
    ~CharacterMovementSystem() override;

    void doInit(fabric::AppContext& ctx) override;
    void doShutdown() override;
    void fixedUpdate(fabric::AppContext& ctx, float fixedDt) override;

    void configureDependencies() override;

    void onWorldBegin();
    void onWorldEnd();

    const fabric::Vec3f& playerPosition() const { return playerPos_; }
    fabric::Vec3f& playerPosition() { return playerPos_; }
    void setPlayerPosition(const fabric::Vec3f& pos);
    const fabric::Vector3<double, fabric::Space::World>& playerWorldPositionD() const { return playerPosD_; }

    const Velocity& playerVelocity() const { return playerVel_; }
    Velocity& playerVelocity() { return playerVel_; }
    const MovementFSM& movementFSM() const { return movementFSM_; }
    MovementFSM& movementFSM() { return movementFSM_; }
    bool isFlying() const { return movementFSM_.isFlying(); }
    const CharacterConfig& charConfig() const { return charConfig_; }

  private:
    TerrainSystem* terrain_ = nullptr;
    CameraGameSystem* camera_ = nullptr;
    PhysicsGameSystem* physics_ = nullptr;
    VoxelSimulationSystem* voxelSim_ = nullptr;

    std::unique_ptr<FlightController> flightCtrl_;
    JoltCharacterController* joltCharCtrl_ = nullptr; // Owned by PhysicsWorld, required
    MovementFSM movementFSM_;
    CharacterConfig charConfig_;
    fabric::Vec3f playerPos_{K_DEFAULT_SPAWN_X, K_DEFAULT_SPAWN_Y, K_DEFAULT_SPAWN_Z};
    fabric::Vector3<double, fabric::Space::World> playerPosD_{static_cast<double>(K_DEFAULT_SPAWN_X),
                                                              static_cast<double>(K_DEFAULT_SPAWN_Y),
                                                              static_cast<double>(K_DEFAULT_SPAWN_Z)};
    Velocity playerVel_{};
    flecs::entity playerEntity_;
    flecs::world* ecsWorld_ = nullptr;
    int streamRadius_ = 8;
    int collisionRadius_ = 4;
};

} // namespace recurse::systems
