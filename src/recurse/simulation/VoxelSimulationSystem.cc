#include "recurse/simulation/VoxelSimulationSystem.hh"
#include "fabric/utils/Profiler.hh"

namespace recurse::simulation {

VoxelSimulationSystem::VoxelSimulationSystem() : sandSystem_(registry_) {}

void VoxelSimulationSystem::tick() {
    FABRIC_ZONE_SCOPED_N("voxel_sim_tick");

    // 1. Collect active + boundary-dirty chunks
    auto active = tracker_.collectActiveChunks();
    FABRIC_ZONE_VALUE(static_cast<int64_t>(active.size()));

    if (active.empty()) {
        // Nothing to simulate; still advance frame
        ++frameIndex_;
        return;
    }

    // Resolve BoundaryDirty -> Active for chunks that need simulation
    for (const auto& entry : active) {
        if (tracker_.getState(entry.pos) == ChunkState::BoundaryDirty)
            tracker_.resolveBoundaryDirty(entry.pos, true);
    }

    // 2. Sync ghost cells for all active chunks
    {
        FABRIC_ZONE_SCOPED_N("ghost_cell_copy");
        std::vector<ChunkPos> positions;
        positions.reserve(active.size());
        for (const auto& entry : active)
            positions.push_back(entry.pos);
        ghosts_.syncAll(positions, grid_);
    }

    // 3. Simulate chunks via worker pool (parallel or inline)
    {
        FABRIC_ZONE_SCOPED_N("chunk_simulate");
        std::vector<std::function<void(std::mt19937&)>> tasks;
        tasks.reserve(active.size());
        for (const auto& entry : active) {
            ChunkPos pos = entry.pos;
            tasks.push_back([this, pos](std::mt19937& rng) {
                sandSystem_.simulateChunk(pos, grid_, ghosts_, tracker_, frameIndex_, rng);
            });
        }
        workerPool_.dispatchAndWait(tasks, frameIndex_);
    }

    // 4. Advance epoch (swap read/write buffers)
    {
        FABRIC_ZONE_SCOPED_N("epoch_advance");
        grid_.advanceEpoch();
    }

    // 5. Propagate dirty: wake neighbors of active chunks
    {
        FABRIC_ZONE_SCOPED_N("dirty_propagate");
        propagateDirty(active);
    }

    // 6. Increment frame
    ++frameIndex_;
}

void VoxelSimulationSystem::propagateDirty(const std::vector<ActiveChunkEntry>& active) {
    for (const auto& entry : active) {
        // If chunk is still active (had movement), notify all 6 face-neighbors
        if (tracker_.getState(entry.pos) == ChunkState::Active) {
            const auto& p = entry.pos;
            tracker_.notifyBoundaryChange(ChunkPos{p.x + 1, p.y, p.z});
            tracker_.notifyBoundaryChange(ChunkPos{p.x - 1, p.y, p.z});
            tracker_.notifyBoundaryChange(ChunkPos{p.x, p.y + 1, p.z});
            tracker_.notifyBoundaryChange(ChunkPos{p.x, p.y - 1, p.z});
            tracker_.notifyBoundaryChange(ChunkPos{p.x, p.y, p.z + 1});
            tracker_.notifyBoundaryChange(ChunkPos{p.x, p.y, p.z - 1});
        }
    }
}

SimulationGrid& VoxelSimulationSystem::grid() {
    return grid_;
}
const SimulationGrid& VoxelSimulationSystem::grid() const {
    return grid_;
}
MaterialRegistry& VoxelSimulationSystem::materials() {
    return registry_;
}
const MaterialRegistry& VoxelSimulationSystem::materials() const {
    return registry_;
}
ChunkActivityTracker& VoxelSimulationSystem::activityTracker() {
    return tracker_;
}
const ChunkActivityTracker& VoxelSimulationSystem::activityTracker() const {
    return tracker_;
}
uint64_t VoxelSimulationSystem::frameIndex() const {
    return frameIndex_;
}
SimWorkerPool& VoxelSimulationSystem::workerPool() {
    return workerPool_;
}

} // namespace recurse::simulation
