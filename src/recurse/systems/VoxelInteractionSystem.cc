#include "recurse/systems/VoxelInteractionSystem.hh"
#include "recurse/systems/CameraGameSystem.hh"
#include "recurse/systems/CharacterMovementSystem.hh"
#include "recurse/systems/TerrainSystem.hh"
#include "recurse/systems/VoxelSimulationSystem.hh"
#include "recurse/world/ChunkCoordUtils.hh"

#include "fabric/core/AppContext.hh"
#include "fabric/core/InputManager.hh"
#include "fabric/core/Log.hh"
#include "fabric/core/SystemRegistry.hh"
#include "fabric/simulation/ChunkActivityTracker.hh"
#include "fabric/simulation/SimulationGrid.hh"
#include "fabric/simulation/VoxelMaterial.hh"
#include "fabric/utils/Profiler.hh"
#include "recurse/gameplay/VoxelInteraction.hh"

namespace recurse::systems {

VoxelInteractionSystem::~VoxelInteractionSystem() = default;

void VoxelInteractionSystem::doShutdown() {
    voxelInteraction_.reset();
    terrain_ = nullptr;
    camera_ = nullptr;
    voxelSim_ = nullptr;
    FABRIC_LOG_INFO("VoxelInteractionSystem shut down");
}

void VoxelInteractionSystem::doInit(fabric::AppContext& ctx) {
    terrain_ = ctx.systemRegistry.get<TerrainSystem>();
    camera_ = ctx.systemRegistry.get<CameraGameSystem>();
    voxelSim_ = ctx.systemRegistry.get<VoxelSimulationSystem>();

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
            if (r.success) {
                interactionCooldown_ = K_INTERACTION_RATE;
                // Sync to SimulationGrid for rendering and notify tracker for remesh
                if (voxelSim_) {
                    using namespace fabric::simulation;
                    voxelSim_->simulationGrid().writeCell(r.x, r.y, r.z,
                                                          VoxelCell{material_ids::AIR, 128, voxel_flags::UPDATED});
                    voxelSim_->activityTracker().notifyBoundaryChange(ChunkPos{r.cx, r.cy, r.cz});
                    // Notify face-adjacent neighbors (they share boundary vertices)
                    for (int i = 0; i < 6; ++i) {
                        ChunkPos neighbor{r.cx + K_FACE_NEIGHBORS[i][0], r.cy + K_FACE_NEIGHBORS[i][1],
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
            auto r = voxelInteraction_->createMatterAt(
                terrain_->densityGrid(), camPos.x, camPos.y, camPos.z, camFwd.x, camFwd.y, camFwd.z, 1.0f,
                fabric::Vector4<float, fabric::Space::World>(0.4f, 0.7f, 0.3f, 1.0f), 10.0f);
            if (r.success) {
                interactionCooldown_ = K_INTERACTION_RATE;
                // Sync to SimulationGrid for rendering and notify tracker for remesh
                if (voxelSim_) {
                    using namespace fabric::simulation;
                    // Use Sand as default placed material (visible, interacts with physics)
                    voxelSim_->simulationGrid().writeCell(r.x, r.y, r.z,
                                                          VoxelCell{material_ids::SAND, 128, voxel_flags::UPDATED});
                    voxelSim_->activityTracker().notifyBoundaryChange(ChunkPos{r.cx, r.cy, r.cz});
                    // Notify face-adjacent neighbors
                    for (int i = 0; i < 6; ++i) {
                        ChunkPos neighbor{r.cx + K_FACE_NEIGHBORS[i][0], r.cy + K_FACE_NEIGHBORS[i][1],
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

} // namespace recurse::systems
