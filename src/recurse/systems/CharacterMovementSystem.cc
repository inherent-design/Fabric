#include "recurse/systems/CharacterMovementSystem.hh"
#include "recurse/systems/PhysicsGameSystem.hh"
#include "recurse/systems/TerrainSystem.hh"

#include "fabric/core/AppContext.hh"
#include "fabric/core/Event.hh"
#include "fabric/core/InputManager.hh"
#include "fabric/core/Log.hh"
#include "fabric/core/SystemRegistry.hh"
#include "recurse/gameplay/CameraController.hh"
#include "recurse/gameplay/CharacterController.hh"
#include "recurse/gameplay/FlightController.hh"

#include <cmath>

// Forward declare; CameraGameSystem is created by W1B but will exist
// in the registry at init time because all systems are registered
// before initAll() runs.
namespace recurse::systems {
class CameraGameSystem;
} // namespace recurse::systems

namespace recurse::systems {

void CharacterMovementSystem::init(fabric::AppContext& ctx) {
    terrain_ = ctx.systemRegistry.get<TerrainSystem>();
    camera_ = ctx.systemRegistry.get<CameraGameSystem>();

    constexpr float kCharWidth = 0.6f;
    constexpr float kCharHeight = 1.8f;
    constexpr float kCharDepth = 0.6f;

    charCtrl_ = std::make_unique<CharacterController>(kCharWidth, kCharHeight, kCharDepth);
    flightCtrl_ = std::make_unique<FlightController>(kCharWidth, kCharHeight, kCharDepth);

    // Toggle fly mode
    ctx.dispatcher.addEventListener("toggle_fly", [this](fabric::Event&) {
        if (movementFSM_.isFlying()) {
            movementFSM_.tryTransition(CharacterState::Falling);
            FABRIC_LOG_INFO("Flight mode: off");
        } else {
            movementFSM_.tryTransition(CharacterState::Flying);
            playerVel_ = {};
            FABRIC_LOG_INFO("Flight mode: on");
        }
    });

    // Jump on space press (grounded only)
    ctx.dispatcher.addEventListener("move_up", [this](fabric::Event&) {
        if (movementFSM_.isGrounded()) {
            movementFSM_.tryTransition(CharacterState::Jumping);
            playerVel_.y = charConfig_.jumpForce;
        }
    });

    FABRIC_LOG_INFO("CharacterMovementSystem initialized");
}

void CharacterMovementSystem::fixedUpdate(fabric::AppContext& ctx, float fixedDt) {
    auto* inputManager = ctx.inputManager;

    // Camera direction from previous frame (CameraGameSystem runs in Update phase).
    // This matches existing behavior: onFixedUpdate read cameraCtrl before
    // onRender updated it.
    // CameraGameSystem provides forward()/right() accessors.
    // Until W2 wiring, use zero vectors as safe fallback.
    fabric::Vec3f camFwd(0.0f, 0.0f, -1.0f);
    fabric::Vec3f camRight(1.0f, 0.0f, 0.0f);

    // camera_ may be null if CameraGameSystem is not yet registered
    // (during unit testing or partial system setup)
    if (camera_) {
        // CameraGameSystem is defined by W1B; access its forward/right
        // through the opaque pointer. The actual method calls will compile
        // once both W1A and W1B files are in CMakeLists.txt.
        // For now, leave the fallback vectors above.
    }

    if (movementFSM_.isFlying()) {
        auto fwd = camFwd;
        auto right = camRight;

        fabric::Vec3f moveDir(0.0f, 0.0f, 0.0f);
        if (inputManager->isActionActive("move_forward"))
            moveDir = moveDir + fwd;
        if (inputManager->isActionActive("move_backward"))
            moveDir = moveDir - fwd;
        if (inputManager->isActionActive("move_right"))
            moveDir = moveDir + right;
        if (inputManager->isActionActive("move_left"))
            moveDir = moveDir - right;
        if (inputManager->isActionActive("move_up"))
            moveDir = moveDir + fabric::Vec3f(0.0f, 1.0f, 0.0f);
        if (inputManager->isActionActive("move_down"))
            moveDir = moveDir - fabric::Vec3f(0.0f, 1.0f, 0.0f);

        float len = std::sqrt(moveDir.x * moveDir.x + moveDir.y * moveDir.y + moveDir.z * moveDir.z);
        if (len > 0.001f)
            moveDir = fabric::Vec3f(moveDir.x / len, moveDir.y / len, moveDir.z / len);

        fabric::Vec3f displacement(moveDir.x * charConfig_.flightSpeed * fixedDt,
                                   moveDir.y * charConfig_.flightSpeed * fixedDt,
                                   moveDir.z * charConfig_.flightSpeed * fixedDt);

        auto result = flightCtrl_->move(playerPos_, displacement, terrain_->densityGrid());
        playerPos_ = result.resolvedPosition;

    } else {
        // Ground mode: flatten forward/right to XZ plane
        auto fwd = camFwd;
        auto right = camRight;

        fabric::Vec3f flatFwd(fwd.x, 0.0f, fwd.z);
        float fwdLen = std::sqrt(flatFwd.x * flatFwd.x + flatFwd.z * flatFwd.z);
        if (fwdLen > 0.001f)
            flatFwd = fabric::Vec3f(flatFwd.x / fwdLen, 0.0f, flatFwd.z / fwdLen);

        fabric::Vec3f flatRight(right.x, 0.0f, right.z);
        float rightLen = std::sqrt(flatRight.x * flatRight.x + flatRight.z * flatRight.z);
        if (rightLen > 0.001f)
            flatRight = fabric::Vec3f(flatRight.x / rightLen, 0.0f, flatRight.z / rightLen);

        fabric::Vec3f horizMove(0.0f, 0.0f, 0.0f);
        if (inputManager->isActionActive("move_forward"))
            horizMove = horizMove + flatFwd;
        if (inputManager->isActionActive("move_backward"))
            horizMove = horizMove - flatFwd;
        if (inputManager->isActionActive("move_right"))
            horizMove = horizMove + flatRight;
        if (inputManager->isActionActive("move_left"))
            horizMove = horizMove - flatRight;

        float horizLen = std::sqrt(horizMove.x * horizMove.x + horizMove.z * horizMove.z);
        if (horizLen > 0.001f)
            horizMove = fabric::Vec3f(horizMove.x / horizLen, 0.0f, horizMove.z / horizLen);

        // Gravity
        playerVel_.y -= charConfig_.gravity * fixedDt;

        fabric::Vec3f displacement(horizMove.x * charConfig_.walkSpeed * fixedDt, playerVel_.y * fixedDt,
                                   horizMove.z * charConfig_.walkSpeed * fixedDt);

        auto result = charCtrl_->move(playerPos_, displacement, terrain_->densityGrid());
        playerPos_ = result.resolvedPosition;

        if (result.onGround) {
            playerVel_.y = 0.0f;
            if (movementFSM_.currentState() == CharacterState::Falling ||
                movementFSM_.currentState() == CharacterState::Jumping) {
                movementFSM_.tryTransition(CharacterState::Grounded);
            }
        } else if (movementFSM_.isGrounded()) {
            movementFSM_.tryTransition(CharacterState::Falling);
        }

        // Ceiling collision: kill upward velocity
        if (result.hitY && playerVel_.y > 0.0f)
            playerVel_.y = 0.0f;
    }
}

void CharacterMovementSystem::configureDependencies() {
    after<PhysicsGameSystem>();
    after<TerrainSystem>();
}

} // namespace recurse::systems
