#include "recurse/systems/VoxelInteractionSystem.hh"
#include "recurse/systems/CharacterMovementSystem.hh"
#include "recurse/systems/TerrainSystem.hh"

#include "fabric/core/AppContext.hh"
#include "fabric/core/InputManager.hh"
#include "fabric/core/Log.hh"
#include "fabric/core/SystemRegistry.hh"
#include "recurse/gameplay/VoxelInteraction.hh"

// Forward declare; CameraGameSystem is created by W1B
namespace recurse::systems {
class CameraGameSystem;
} // namespace recurse::systems

namespace recurse::systems {

void VoxelInteractionSystem::init(fabric::AppContext& ctx) {
    terrain_ = ctx.systemRegistry.get<TerrainSystem>();
    camera_ = ctx.systemRegistry.get<CameraGameSystem>();

    voxelInteraction_ = std::make_unique<VoxelInteraction>(terrain_->density(), terrain_->essence(), ctx.dispatcher);

    FABRIC_LOG_INFO("VoxelInteractionSystem initialized");
}

void VoxelInteractionSystem::fixedUpdate(fabric::AppContext& ctx, float fixedDt) {
    auto* inputManager = ctx.inputManager;

    interactionCooldown_ -= fixedDt;
    if (interactionCooldown_ <= 0.0f) {
        // Camera position/forward for raycasts.
        // Until W2 wiring connects CameraGameSystem, use safe defaults.
        fabric::Vec3f camPos(16.0f, 48.0f, 16.0f);
        fabric::Vec3f camFwd(0.0f, 0.0f, -1.0f);

        if (camera_) {
            // Will be wired in Wave 2: camPos = camera_->position();
            // camFwd = camera_->forward();
        }

        if (inputManager->mouseButton(1)) {
            auto r = voxelInteraction_->destroyMatterAt(terrain_->densityGrid(), camPos.x, camPos.y, camPos.z, camFwd.x,
                                                        camFwd.y, camFwd.z, 10.0f);
            if (r.success)
                interactionCooldown_ = kInteractionRate;
        }
        if (inputManager->mouseButton(3)) {
            auto r = voxelInteraction_->createMatterAt(
                terrain_->densityGrid(), camPos.x, camPos.y, camPos.z, camFwd.x, camFwd.y, camFwd.z, 1.0f,
                fabric::Vector4<float, fabric::Space::World>(0.4f, 0.7f, 0.3f, 1.0f), 10.0f);
            if (r.success)
                interactionCooldown_ = kInteractionRate;
        }
    }
}

void VoxelInteractionSystem::configureDependencies() {
    after<TerrainSystem>();
    after<CharacterMovementSystem>();
}

} // namespace recurse::systems
