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

    // 2b. Pre-materialize face-neighbor chunks so writeCell cannot
    // insert into chunks_ during parallel dispatch (ARCH-SIM-RACE fix).
    {
        FABRIC_ZONE_SCOPED_N("pre_materialize_neighbors");
        const int offsets[6][3] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
        for (const auto& entry : active) {
            const auto& p = entry.pos;
            for (const auto& off : offsets) {
                int nx = p.x + off[0], ny = p.y + off[1], nz = p.z + off[2];
                if (grid_.hasChunk(nx, ny, nz))
                    grid_.materializeChunk(nx, ny, nz);
            }
        }
    }

    // 3. Simulate chunks via scheduler (parallel or inline)
    std::vector<BoundaryWriteQueue> boundaryQueues(scheduler_.workerCount() + 1);
    {
        FABRIC_ZONE_SCOPED_N("chunk_simulate");
        scheduler_.parallelFor(active.size(), [&](size_t jobIdx, size_t workerIdx) {
            std::mt19937 rng(frameIndex_ + jobIdx);
            const auto& pos = active[jobIdx].pos;
            sandSystem_.simulateChunk(pos, grid_, ghosts_, tracker_, frameIndex_, rng, boundaryQueues[workerIdx]);
        });
    }

    // 3b. Drain boundary write queues (single-threaded)
    {
        FABRIC_ZONE_SCOPED_N("boundary_drain");
        drainBoundaryWrites(boundaryQueues);
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

void VoxelSimulationSystem::drainBoundaryWrites(std::vector<BoundaryWriteQueue>& queues) {
    for (auto& queue : queues) {
        for (const auto& bw : queue) {
            if (grid_.writeCellIfExists(bw.dstWx, bw.dstWy, bw.dstWz, bw.writeCell)) {
                tracker_.notifyBoundaryChange(bw.neighborChunk);
            } else {
                grid_.writeCell(bw.srcWx, bw.srcWy, bw.srcWz, bw.undoCell);
            }
        }
        queue.clear();
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
fabric::JobScheduler& VoxelSimulationSystem::scheduler() {
    return scheduler_;
}

} // namespace recurse::simulation
