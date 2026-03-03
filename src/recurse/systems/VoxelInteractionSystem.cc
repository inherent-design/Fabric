#include "recurse/systems/VoxelInteractionSystem.hh"
#include "recurse/systems/CameraGameSystem.hh"
#include "recurse/systems/CharacterMovementSystem.hh"
#include "recurse/systems/TerrainSystem.hh"

#include "fabric/core/AppContext.hh"
#include "fabric/core/InputManager.hh"
#include "fabric/core/Log.hh"
#include "fabric/core/SystemRegistry.hh"
#include "fabric/utils/Profiler.hh"
#include "recurse/gameplay/VoxelInteraction.hh"

namespace recurse::systems {

VoxelInteractionSystem::~VoxelInteractionSystem() = default;

void VoxelInteractionSystem::init(fabric::AppContext& ctx) {
    terrain_ = ctx.systemRegistry.get<TerrainSystem>();
    camera_ = ctx.systemRegistry.get<CameraGameSystem>();

    voxelInteraction_ = std::make_unique<VoxelInteraction>(terrain_->density(), terrain_->essence(), ctx.dispatcher);

    FABRIC_LOG_INFO("VoxelInteractionSystem initialized");
}

void VoxelInteractionSystem::fixedUpdate(fabric::AppContext& ctx, float fixedDt) {
    FABRIC_ZONE_SCOPED_N("voxel_interaction");
    auto* inputManager = ctx.inputManager;

    interactionCooldown_ -= fixedDt;
    if (interactionCooldown_ <= 0.0f) {
        if (!camera_) {
            FABRIC_LOG_WARN("VoxelInteractionSystem: camera not available, skipping interaction");
            return;
        }

        fabric::Vec3f camPos = camera_->position();
        fabric::Vec3f camFwd = camera_->forward();

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
    after<CameraGameSystem>();
}

} // namespace recurse::systems
