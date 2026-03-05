#pragma once

#include <Jolt/Jolt.h>

#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>
#include <Jolt/Physics/PhysicsSystem.h>

#include "fabric/core/Rendering.hh"
#include "fabric/core/Spatial.hh"

#include <memory>

namespace recurse {

/// Configuration for creating a JoltCharacterController.
/// Maps to existing CharacterController dimensions.
struct JoltCharacterConfig {
    float width = 0.6f;                    ///< Character capsule/box width (XZ diameter)
    float height = 1.8f;                   ///< Character total height
    float mass = 70.0f;                    ///< Character mass (kg)
    float maxStrength = 100.0f;            ///< Maximum push force
    float characterPadding = 0.02f;        ///< Skin width for collision
    float penetrationRecoverySpeed = 1.0f; ///< How fast character recovers from penetration
    float maxSlopeAngle = 0.707f;          ///< cos(45 degrees) - max walkable slope
};

/// Wraps JPH::CharacterVirtual for use with the existing movement system.
/// Replaces CharacterController and FlightController with unified Jolt collision.
class JoltCharacterController {
  public:
    struct CollisionResult {
        bool hitAny = false;
        bool hitY = false; ///< Collision on Y axis (ceiling or floor)
        bool onGround = false;
        fabric::Vec3f resolvedPosition;
        fabric::Vec3f velocity;
    };

    /// Create a CharacterVirtual tied to the given physics system.
    /// @param system The Jolt physics system (must outlive this controller)
    /// @param config Character dimensions and physics settings
    JoltCharacterController(JPH::PhysicsSystem* system, const JoltCharacterConfig& config);
    ~JoltCharacterController();

    // Non-copyable, movable
    JoltCharacterController(const JoltCharacterController&) = delete;
    JoltCharacterController& operator=(const JoltCharacterController&) = delete;
    JoltCharacterController(JoltCharacterController&&) noexcept;
    JoltCharacterController& operator=(JoltCharacterController&&) noexcept;

    /// Main update - equivalent to CharacterController::move().
    /// Uses CharacterVirtual::Update() for collision resolution.
    /// @param currentPos Starting position
    /// @param velocity Desired velocity (will be modified by collision)
    /// @param deltaTime Time step
    /// @param allocator Temp allocator from PhysicsWorld
    /// @return Collision result with resolved position and ground state
    CollisionResult move(const fabric::Vec3f& currentPos, const fabric::Vec3f& velocity, float deltaTime,
                         JPH::TempAllocator& allocator);

    /// Ground state queries - map directly to Jolt CharacterVirtual states.
    bool isOnGround() const;
    bool canWalkStairs() const;

    /// Position/velocity accessors.
    fabric::Vec3f getPosition() const;
    void setPosition(const fabric::Vec3f& pos);
    fabric::Vec3f getLinearVelocity() const;
    void setLinearVelocity(const fabric::Vec3f& vel);

    /// Shape management (for stance changes like crouching).
    /// @param shape New shape to use
    /// @param maxPenetration Maximum allowed penetration for shape change
    /// @param allocator Temp allocator for collision queries
    /// @return true if shape change succeeded
    bool setShape(const JPH::Shape* shape, float maxPenetration, JPH::TempAllocator& allocator);

    /// Get the underlying Jolt CharacterVirtual (for advanced use).
    JPH::CharacterVirtual* character() { return character_.get(); }
    const JPH::CharacterVirtual* character() const { return character_.get(); }

  private:
    std::unique_ptr<JPH::CharacterVirtual> character_;
    JPH::PhysicsSystem* physicsSystem_ = nullptr;

    // Track previous frame's ground state for edge detection
    bool wasOnGround_ = false;
};

} // namespace recurse
