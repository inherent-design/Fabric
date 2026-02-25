#pragma once

#include <cstdint>

namespace fabric {

enum class CharacterState : uint8_t {
    Grounded,
    Falling,
    Jumping,
    Climbing,
    Swimming,
    WallRunning,
    Hanging,
    Flying,
    Sliding,
    Ragdoll,
    Dashing,
    Boosting
};

// ECS components (POD structs for Flecs)

struct Velocity {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct CharacterStateComponent {
    CharacterState state = CharacterState::Grounded;
};

struct CharacterConfig {
    float walkSpeed = 5.0f;
    float runSpeed = 10.0f;
    float jumpForce = 8.0f;
    float gravity = 20.0f;
    float stepHeight = 1.0f;
    float slopeLimit = 0.707f; // cos(45 degrees)
    float flightSpeed = 15.0f;
    float dashSpeed = 25.0f;
    float dashDuration = 0.25f;
    float dashCooldown = 1.5f;
    float boostSpeed = 30.0f;
    float boostCooldown = 2.0f;
};

struct DashState {
    float cooldownRemaining = 0.0f;
    float durationRemaining = 0.0f;
    bool active = false;
};

} // namespace fabric
