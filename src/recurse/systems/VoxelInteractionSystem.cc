#include "recurse/systems/VoxelInteractionSystem.hh"
#include "fabric/core/WorldLifecycle.hh"
#include "fabric/world/ChunkCoordUtils.hh"
#include "recurse/systems/CameraGameSystem.hh"
#include "recurse/systems/CharacterMovementSystem.hh"
#include "recurse/systems/TerrainSystem.hh"
#include "recurse/systems/VoxelSimulationSystem.hh"

#include "fabric/core/AppContext.hh"
#include "fabric/core/SystemRegistry.hh"
#include "fabric/input/InputManager.hh"
#include "fabric/log/Log.hh"
#include "fabric/utils/Profiler.hh"
#include "recurse/character/VoxelInteraction.hh"
#include "recurse/simulation/ChunkActivityTracker.hh"
#include "recurse/simulation/SimulationGrid.hh"
#include "recurse/simulation/VoxelMaterial.hh"

namespace recurse::systems {

using fabric::K_FACE_NEIGHBORS;

VoxelInteractionSystem::~VoxelInteractionSystem() = default;

void VoxelInteractionSystem::doShutdown() {
    voxelInteraction_.reset();
    terrain_ = nullptr;
    camera_ = nullptr;
    voxelSim_ = nullptr;
    FABRIC_LOG_INFO("VoxelInteractionSystem shut down");
}

void VoxelInteractionSystem::doInit(fabric::AppContext& ctx) {
    if (auto* wl = ctx.worldLifecycle) {
        wl->registerParticipant([this]() { onWorldBegin(); }, [this]() { onWorldEnd(); });
    }
    terrain_ = ctx.systemRegistry.get<TerrainSystem>();
    camera_ = ctx.systemRegistry.get<CameraGameSystem>();
    voxelSim_ = ctx.systemRegistry.get<VoxelSimulationSystem>();

    voxelInteraction_ = std::make_unique<VoxelInteraction>(voxelSim_->simulationGrid(), ctx.dispatcher);

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
            auto r =
                voxelInteraction_->destroyMatterAt(camPos.x, camPos.y, camPos.z, camFwd.x, camFwd.y, camFwd.z, 10.0f);
            if (r.success) {
                interactionCooldown_ = K_INTERACTION_RATE;
                if (voxelSim_) {
                    using namespace recurse::simulation;
                    voxelSim_->activityTracker().notifyBoundaryChange(ChunkCoord{r.cx, r.cy, r.cz});
                    for (int i = 0; i < 6; ++i) {
                        ChunkCoord neighbor{r.cx + K_FACE_NEIGHBORS[i][0], r.cy + K_FACE_NEIGHBORS[i][1],
                                            r.cz + K_FACE_NEIGHBORS[i][2]};
                        if (voxelSim_->simulationGrid().hasChunk(neighbor.x, neighbor.y, neighbor.z)) {
                            voxelSim_->activityTracker().notifyBoundaryChange(neighbor);
                        }
                    }
                    FABRIC_LOG_DEBUG("VoxelInteraction: destroyed at ({},{},{}) chunk ({},{},{})", r.x, r.y, r.z, r.cx,
                                     r.cy, r.cz);
                }
            }
        }
        if (inputManager->mouseButton(3)) {
            auto r = voxelInteraction_->createMatterAt(camPos.x, camPos.y, camPos.z, camFwd.x, camFwd.y, camFwd.z,
                                                       recurse::simulation::material_ids::SAND, 10.0f);
            if (r.success) {
                interactionCooldown_ = K_INTERACTION_RATE;
                if (voxelSim_) {
                    using namespace recurse::simulation;
                    // Target chunk needs Active (not BoundaryDirty) so
                    // FallingSandSystem processes it before meshing sleeps it.
                    voxelSim_->activityTracker().setState(ChunkCoord{r.cx, r.cy, r.cz}, ChunkState::Active);
                    for (int i = 0; i < 6; ++i) {
                        ChunkCoord neighbor{r.cx + K_FACE_NEIGHBORS[i][0], r.cy + K_FACE_NEIGHBORS[i][1],
                                            r.cz + K_FACE_NEIGHBORS[i][2]};
                        if (voxelSim_->simulationGrid().hasChunk(neighbor.x, neighbor.y, neighbor.z)) {
                            voxelSim_->activityTracker().notifyBoundaryChange(neighbor);
                        }
                    }
                    FABRIC_LOG_DEBUG("VoxelInteraction: placed Sand at ({},{},{}) chunk ({},{},{})", r.x, r.y, r.z,
                                     r.cx, r.cy, r.cz);
                }
            }
        }
    }
}

void VoxelInteractionSystem::configureDependencies() {
    after<TerrainSystem>();
    after<CharacterMovementSystem>();
    after<CameraGameSystem>();
    after<VoxelSimulationSystem>();
}

void VoxelInteractionSystem::onWorldBegin() {
    // Interaction state is reset reactively; no initialization needed.
}

void VoxelInteractionSystem::onWorldEnd() {
    interactionCooldown_ = 0.0f;
}

} // namespace recurse::systems
