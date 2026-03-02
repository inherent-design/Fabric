#pragma once

#include "fabric/core/Rendering.hh"
#include "fabric/core/SystemBase.hh"
#include "recurse/gameplay/CharacterTypes.hh"
#include "recurse/gameplay/MovementFSM.hh"
#include <memory>

namespace recurse {
class CharacterController;
class FlightController;
class CameraController;
} // namespace recurse

namespace recurse::systems {

class TerrainSystem;
class CameraGameSystem;

/// Owns player movement state: controllers, FSM, position, velocity.
/// Reads camera direction for movement input and resolves character
/// collision against the density grid each fixed tick.
class CharacterMovementSystem : public fabric::System<CharacterMovementSystem> {
  public:
    CharacterMovementSystem() = default;
    ~CharacterMovementSystem() override;

    void init(fabric::AppContext& ctx) override;
    void fixedUpdate(fabric::AppContext& ctx, float fixedDt) override;

    void configureDependencies() override;

    const fabric::Vec3f& playerPosition() const { return playerPos_; }
    fabric::Vec3f& playerPosition() { return playerPos_; }
    const Velocity& playerVelocity() const { return playerVel_; }
    Velocity& playerVelocity() { return playerVel_; }
    const MovementFSM& movementFSM() const { return movementFSM_; }
    MovementFSM& movementFSM() { return movementFSM_; }
    bool isFlying() const { return movementFSM_.isFlying(); }
    const CharacterConfig& charConfig() const { return charConfig_; }

  private:
    static constexpr float kSpawnX = 16.0f;
    static constexpr float kSpawnY = 48.0f;
    static constexpr float kSpawnZ = 16.0f;

    TerrainSystem* terrain_ = nullptr;
    CameraGameSystem* camera_ = nullptr;

    std::unique_ptr<CharacterController> charCtrl_;
    std::unique_ptr<FlightController> flightCtrl_;
    MovementFSM movementFSM_;
    CharacterConfig charConfig_;
    fabric::Vec3f playerPos_{kSpawnX, kSpawnY, kSpawnZ};
    Velocity playerVel_{};
};

} // namespace recurse::systems
