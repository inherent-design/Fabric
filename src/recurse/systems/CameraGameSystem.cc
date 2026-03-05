#include "recurse/systems/CameraGameSystem.hh"

#include "fabric/core/AppContext.hh"
#include "fabric/core/Camera.hh"
#include "fabric/core/Event.hh"
#include "fabric/core/InputManager.hh"
#include "fabric/core/Log.hh"
#include "fabric/core/SystemRegistry.hh"
#include "recurse/systems/CharacterMovementSystem.hh"
#include "recurse/systems/TerrainSystem.hh"

namespace recurse::systems {

void CameraGameSystem::init(fabric::AppContext& ctx) {
    cameraCtrl_ = std::make_unique<recurse::CameraController>(*ctx.camera);

    charMovement_ = ctx.systemRegistry.get<CharacterMovementSystem>();
    terrain_ = ctx.systemRegistry.get<TerrainSystem>();

    // Toggle between first/third person camera
    ctx.dispatcher.addEventListener("toggle_camera", [this](fabric::Event&) {
        if (cameraCtrl_->mode() == recurse::CameraMode::FirstPerson) {
            cameraCtrl_->setMode(recurse::CameraMode::ThirdPerson);
        } else {
            cameraCtrl_->setMode(recurse::CameraMode::FirstPerson);
        }
    });

    FABRIC_LOG_INFO("CameraGameSystem initialized");
}

void CameraGameSystem::update(fabric::AppContext& ctx, float dt) {
    // Mouse look runs once per frame in Update (was in FixedUpdate behind a
    // once-per-frame guard; the guard is no longer needed here)
    cameraCtrl_->processMouseInput(ctx.inputManager->mouseDeltaX(), ctx.inputManager->mouseDeltaY());

    // Track player position with spring arm collision for third-person mode
    auto playerPosD = charMovement_->playerWorldPositionD();
    cameraCtrl_->update(playerPosD, dt, &terrain_->densityGrid());
}

void CameraGameSystem::shutdown() {
    cameraCtrl_.reset();
    FABRIC_LOG_INFO("CameraGameSystem shut down");
}

void CameraGameSystem::configureDependencies() {
    after<CharacterMovementSystem>();
    after<TerrainSystem>();
}

fabric::Vector3<float, fabric::Space::World> CameraGameSystem::position() const {
    return cameraCtrl_->position();
}

fabric::Vector3<float, fabric::Space::World> CameraGameSystem::forward() const {
    return cameraCtrl_->forward();
}

fabric::Vector3<float, fabric::Space::World> CameraGameSystem::right() const {
    return cameraCtrl_->right();
}

fabric::Vector3<float, fabric::Space::World> CameraGameSystem::up() const {
    return cameraCtrl_->up();
}

} // namespace recurse::systems
