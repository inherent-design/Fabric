#include "recurse/systems/VoxelSimulationSystem.hh"
#include "fabric/world/ChunkCoordUtils.hh"

#include "fabric/core/AppContext.hh"
#include "fabric/core/Event.hh"
#include "fabric/core/Log.hh"
#include "fabric/core/SystemRegistry.hh"
#include "fabric/utils/Profiler.hh"
#include "recurse/character/VoxelInteraction.hh"
#include "recurse/simulation/ChunkRegistry.hh"
#include "recurse/simulation/VoxelSimulationSystem.hh"
#include "recurse/systems/ChunkPipelineSystem.hh"
#include "recurse/systems/TerrainSystem.hh"
#include "recurse/world/WorldGenerator.hh"

namespace recurse::systems {

using fabric::K_FACE_DIAGONAL_NEIGHBORS;

VoxelSimulationSystem::VoxelSimulationSystem() = default;
VoxelSimulationSystem::~VoxelSimulationSystem() = default;

void VoxelSimulationSystem::doInit(fabric::AppContext& ctx) {
    terrain_ = ctx.systemRegistry.get<TerrainSystem>();
    dispatcher_ = &ctx.dispatcher;
    fabSim_ = std::make_unique<recurse::simulation::VoxelSimulationSystem>();

    // Defer terrain generation until TerrainSystem has generated the world
    // This allows MainMenuSystem to select world type first
    FABRIC_LOG_INFO("VoxelSimulationSystem initialized (awaiting world generation)");
}

void VoxelSimulationSystem::doShutdown() {
    fabSim_.reset();
    terrain_ = nullptr;
    dispatcher_ = nullptr;
}

void VoxelSimulationSystem::fixedUpdate(fabric::AppContext& /*ctx*/, float /*fixedDt*/) {
    FABRIC_ZONE_SCOPED_N("recurse_voxel_sim");
    if (!fabSim_)
        return;

    lastActiveCount_ = fabSim_->activityTracker().collectActiveChunks().size();
    FABRIC_ZONE_VALUE(static_cast<int64_t>(lastActiveCount_));
    fabSim_->tick();
}

void VoxelSimulationSystem::configureDependencies() {
    after<TerrainSystem>();
    after<ChunkPipelineSystem>();
}

void VoxelSimulationSystem::generateInitialWorld() {
    if (!terrain_ || !fabSim_ || !dispatcher_) {
        FABRIC_LOG_ERROR("VoxelSimulationSystem: Cannot generate world - not initialized");
        return;
    }

    // Generate initial terrain into the simulation grid.
    // Use 5x3x5 region so inner 3x3x1 chunks at y=0 have all 6 neighbors for meshing.
    auto& gen = terrain_->worldGenerator();
    auto& grid = fabSim_->grid();
    auto& tracker = fabSim_->activityTracker();

    // Track which chunks we generate for collision initialization
    std::vector<std::tuple<int, int, int>> generatedChunks;

    for (int cz = -2; cz <= 2; ++cz) {
        for (int cy = -1; cy <= 1; ++cy) {
            for (int cx = -2; cx <= 2; ++cx) {
                grid.registry().addChunk(cx, cy, cz);
                grid.registry().transitionState(cx, cy, cz, recurse::simulation::ChunkSlotState::Generating);
                gen.generate(grid, cx, cy, cz);
                grid.syncChunkBuffers(cx, cy, cz);
                grid.registry().transitionState(cx, cy, cz, recurse::simulation::ChunkSlotState::Active);
                tracker.setState(recurse::simulation::ChunkPos{cx, cy, cz}, recurse::simulation::ChunkState::Active);
                generatedChunks.emplace_back(cx, cy, cz);
            }
        }
    }

    // Initialize Jolt physics collision for each chunk
    for (const auto& [cx, cy, cz] : generatedChunks) {
        fabric::Event e(K_VOXEL_CHANGED_EVENT, "VoxelSimulationSystem");
        e.setData("cx", cx);
        e.setData("cy", cy);
        e.setData("cz", cz);
        dispatcher_->dispatchEvent(e);
    }

    FABRIC_LOG_INFO("VoxelSimulationSystem: World generated ({} chunks in grid, collision synced)",
                    grid.allChunks().size());
}

recurse::simulation::SimulationGrid& VoxelSimulationSystem::simulationGrid() {
    return fabSim_->grid();
}

const recurse::simulation::SimulationGrid& VoxelSimulationSystem::simulationGrid() const {
    return fabSim_->grid();
}

recurse::simulation::ChunkActivityTracker& VoxelSimulationSystem::activityTracker() {
    return fabSim_->activityTracker();
}

const recurse::simulation::ChunkActivityTracker& VoxelSimulationSystem::activityTracker() const {
    return fabSim_->activityTracker();
}

const recurse::simulation::MaterialRegistry& VoxelSimulationSystem::materials() const {
    return fabSim_->materials();
}

fabric::JobScheduler& VoxelSimulationSystem::scheduler() {
    return fabSim_->scheduler();
}

void VoxelSimulationSystem::generateChunk(int cx, int cy, int cz) {
    if (!terrain_ || !fabSim_)
        return;
    auto& gen = terrain_->worldGenerator();
    auto& grid = fabSim_->grid();
    grid.registry().addChunk(cx, cy, cz);
    grid.registry().transitionState(cx, cy, cz, recurse::simulation::ChunkSlotState::Generating);
    gen.generate(grid, cx, cy, cz);
    grid.syncChunkBuffers(cx, cy, cz);
    grid.registry().transitionState(cx, cy, cz, recurse::simulation::ChunkSlotState::Active);
    fabSim_->activityTracker().setState(recurse::simulation::ChunkPos{cx, cy, cz},
                                        recurse::simulation::ChunkState::Active);

    // Initialize Jolt physics collision for this chunk (always dispatch, even for sentinels)
    if (dispatcher_) {
        fabric::Event e(K_VOXEL_CHANGED_EVENT, "VoxelSimulationSystem");
        e.setData("cx", cx);
        e.setData("cy", cy);
        e.setData("cz", cz);
        dispatcher_->dispatchEvent(e);
    }

    // Notify all face-adjacent AND diagonal (in X/Z plane) existing chunks so they re-mesh boundaries.
    // Corner vertices sample across diagonal chunks, so diagonal neighbors must also remesh.
    int notifiedCount = 0;
    for (int d = 0; d < 10; ++d) {
        recurse::simulation::ChunkPos neighbor{cx + K_FACE_DIAGONAL_NEIGHBORS[d][0],
                                               cy + K_FACE_DIAGONAL_NEIGHBORS[d][1],
                                               cz + K_FACE_DIAGONAL_NEIGHBORS[d][2]};
        if (fabSim_->grid().hasChunk(neighbor.x, neighbor.y, neighbor.z)) {
            fabSim_->activityTracker().notifyBoundaryChange(neighbor);
            ++notifiedCount;
        }
    }
    if (notifiedCount > 0) {
        FABRIC_LOG_DEBUG("Notified {} existing neighbors (face+diagonal) after loading chunk ({},{},{})", notifiedCount,
                         cx, cy, cz);
    }
}

void VoxelSimulationSystem::removeChunk(int cx, int cy, int cz) {
    if (!fabSim_)
        return;
    auto* slot = fabSim_->grid().registry().find(cx, cy, cz);
    if (slot) {
        fabSim_->grid().registry().transitionState(cx, cy, cz, recurse::simulation::ChunkSlotState::Draining);
    }
    fabSim_->grid().removeChunk(cx, cy, cz);
    fabSim_->activityTracker().remove(recurse::simulation::ChunkPos{cx, cy, cz});
}

void VoxelSimulationSystem::resetWorld() {
    if (!fabSim_)
        return;

    // Clear all simulation state
    fabSim_->grid().clear();
    fabSim_->activityTracker().clear();
    lastActiveCount_ = 0;

    FABRIC_LOG_INFO("VoxelSimulationSystem: World reset (grid and tracker cleared)");
}

} // namespace recurse::systems
