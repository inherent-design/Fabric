#include "recurse/systems/VoxelSimulationSystem.hh"

#include "fabric/core/AppContext.hh"
#include "fabric/core/Log.hh"
#include "fabric/core/SystemRegistry.hh"
#include "fabric/simulation/VoxelSimulationSystem.hh"
#include "fabric/utils/Profiler.hh"
#include "recurse/systems/TerrainSystem.hh"
#include "recurse/world/WorldGenerator.hh"

namespace recurse::systems {

VoxelSimulationSystem::VoxelSimulationSystem() = default;
VoxelSimulationSystem::~VoxelSimulationSystem() = default;

void VoxelSimulationSystem::init(fabric::AppContext& ctx) {
    terrain_ = ctx.systemRegistry.get<TerrainSystem>();
    fabSim_ = std::make_unique<fabric::simulation::VoxelSimulationSystem>();

    // Generate initial terrain into the simulation grid
    auto& gen = terrain_->worldGenerator();
    auto& grid = fabSim_->grid();
    auto& tracker = fabSim_->activityTracker();
    for (int cz = -1; cz <= 1; ++cz) {
        for (int cy = -1; cy <= 1; ++cy) {
            for (int cx = -1; cx <= 1; ++cx) {
                gen.generate(grid, cx, cy, cz);
                // Mark chunk Active so VoxelMeshingSystem will mesh it
                tracker.setState(fabric::simulation::ChunkPos{cx, cy, cz}, fabric::simulation::ChunkState::Active);
            }
        }
    }
    grid.advanceEpoch();

    FABRIC_LOG_INFO("VoxelSimulationSystem initialized ({} chunks in grid)", grid.allChunks().size());
}

void VoxelSimulationSystem::shutdown() {
    fabSim_.reset();
    terrain_ = nullptr;
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
}

fabric::simulation::SimulationGrid& VoxelSimulationSystem::simulationGrid() {
    return fabSim_->grid();
}

const fabric::simulation::SimulationGrid& VoxelSimulationSystem::simulationGrid() const {
    return fabSim_->grid();
}

fabric::simulation::ChunkActivityTracker& VoxelSimulationSystem::activityTracker() {
    return fabSim_->activityTracker();
}

const fabric::simulation::ChunkActivityTracker& VoxelSimulationSystem::activityTracker() const {
    return fabSim_->activityTracker();
}

void VoxelSimulationSystem::generateChunk(int cx, int cy, int cz) {
    if (!terrain_ || !fabSim_)
        return;
    auto& gen = terrain_->worldGenerator();
    gen.generate(fabSim_->grid(), cx, cy, cz);
    fabSim_->grid().advanceEpoch();
    fabSim_->activityTracker().setState(fabric::simulation::ChunkPos{cx, cy, cz},
                                        fabric::simulation::ChunkState::Active);

    // Notify all 6 face-adjacent existing chunks so they re-mesh boundaries
    constexpr int kFaceOffsets[6][3] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
    for (int d = 0; d < 6; ++d) {
        fabric::simulation::ChunkPos neighbor{cx + kFaceOffsets[d][0], cy + kFaceOffsets[d][1],
                                              cz + kFaceOffsets[d][2]};
        if (fabSim_->grid().hasChunk(neighbor.x, neighbor.y, neighbor.z)) {
            fabSim_->activityTracker().notifyBoundaryChange(neighbor);
        }
    }
}

void VoxelSimulationSystem::removeChunk(int cx, int cy, int cz) {
    if (!fabSim_)
        return;
    fabSim_->grid().removeChunk(cx, cy, cz);
    fabSim_->activityTracker().remove(fabric::simulation::ChunkPos{cx, cy, cz});
}

} // namespace recurse::systems
