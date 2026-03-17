#include "recurse/simulation/VoxelSimulationSystem.hh"
#include "fabric/utils/Profiler.hh"

namespace recurse::simulation {

VoxelSimulationSystem::VoxelSimulationSystem() : sandSystem_(registry_) {}

void VoxelSimulationSystem::tick() {
    FABRIC_ZONE_SCOPED_N("voxel_sim_tick");

    settledChunks_.clear();

    // 1. Collect active + boundary-dirty chunks, then filter.
    // BoundaryDirty chunks need remeshing only (not simulation).
    // Leave them for VoxelMeshingSystem; simulate only Active chunks.
    auto collected = tracker_.collectActiveChunks();
    std::vector<ActiveChunkEntry> active;
    active.reserve(collected.size());
    for (const auto& entry : collected) {
        if (tracker_.getState(entry.pos) == ChunkState::Active)
            active.push_back(entry);
    }
    FABRIC_ZONE_VALUE(static_cast<int64_t>(active.size()));

    if (active.empty()) {
        // Nothing to simulate; still advance frame
        ++frameIndex_;
        return;
    }

    // Phase 0: Resolve buffer pointers and build dispatch list
    {
        FABRIC_ZONE_SCOPED_N("phase_0_dispatch");
        grid_.registry().resolveBufferPointers(grid_.currentEpoch());
        // Dispatch list not yet consumed by simulation; collectActiveChunks still
        // drives the work list via ChunkState. Phase 0 wiring prepares for future
        // migration where dispatchList replaces collectActiveChunks.
        auto dispatchList = grid_.registry().buildDispatchList(ChunkSlotState::Active);
        (void)dispatchList;
    }

    // Phase 2: Sync ghost cells for all active chunks
    {
        FABRIC_ZONE_SCOPED_N("phase_2_ghost_sync");
        std::vector<ChunkCoord> positions;
        positions.reserve(active.size());
        for (const auto& entry : active)
            positions.push_back(entry.pos);
        ghosts_.syncAll(positions, grid_);
    }

    // Phase 2b: Pre-materialize face-neighbor chunks so writeCell cannot
    // insert into chunks_ during parallel dispatch (ARCH-SIM-RACE fix).
    {
        FABRIC_ZONE_SCOPED_N("phase_2b_pre_materialize");
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

    // Phase 3: Simulate chunks via scheduler (parallel or inline)
    size_t workerSlots = scheduler_.workerCount() + 1;
    std::vector<BoundaryWriteQueue> boundaryQueues(workerSlots);
    std::vector<std::vector<ChunkCoord>> settledPerWorker(workerSlots);
    std::vector<std::vector<CellSwap>> cellSwapsPerWorker(workerSlots);
    {
        FABRIC_ZONE_SCOPED_N("phase_3_simulate");
        scheduler_.parallelFor(active.size(), [&](size_t jobIdx, size_t workerIdx) {
            const auto& pos = active[jobIdx].pos;
            std::mt19937 rng(static_cast<uint32_t>(worldSeed_ ^ spatialHash(pos)));
            bool reverseDir = (spatialHash(pos) & 1) != 0;
            bool settled = sandSystem_.simulateChunk(pos, grid_, ghosts_, tracker_, reverseDir, rng,
                                                     boundaryQueues[workerIdx], cellSwapsPerWorker[workerIdx]);
            if (settled) {
                settledPerWorker[workerIdx].push_back(pos);
            } else {
                auto* slot = grid_.registry().find(pos.x, pos.y, pos.z);
                if (slot)
                    slot->copyCountdown = ChunkBuffers::K_COUNT - 1;
            }
        });
    }

    // Merge settled lists and cell swaps (single-threaded)
    for (auto& v : settledPerWorker) {
        settledChunks_.insert(settledChunks_.end(), v.begin(), v.end());
    }
    {
        std::unordered_map<ChunkCoord, uint32_t, ChunkCoordHash> swapCounts;
        for (const auto& swaps : cellSwapsPerWorker) {
            for (const auto& swap : swaps)
                swapCounts[swap.chunk]++;
        }
        for (const auto& [pos, count] : swapCounts)
            velocityTracker_.record(pos, count, frameIndex_);
    }

    // Phase 3b: Drain boundary write queues (single-threaded)
    {
        FABRIC_ZONE_SCOPED_N("phase_3b_boundary_drain");
        drainBoundaryWrites(boundaryQueues);
    }

    // Phase 4: Advance epoch (swap read/write buffers)
    {
        FABRIC_ZONE_SCOPED_N("phase_4_epoch_advance");
        grid_.advanceEpoch();
    }

    // Phase 5: Propagate dirty; wake neighbors of active chunks
    {
        FABRIC_ZONE_SCOPED_N("phase_5_dirty_propagate");
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
            tracker_.notifyBoundaryChange(ChunkCoord{p.x + 1, p.y, p.z});
            tracker_.notifyBoundaryChange(ChunkCoord{p.x - 1, p.y, p.z});
            tracker_.notifyBoundaryChange(ChunkCoord{p.x, p.y + 1, p.z});
            tracker_.notifyBoundaryChange(ChunkCoord{p.x, p.y - 1, p.z});
            tracker_.notifyBoundaryChange(ChunkCoord{p.x, p.y, p.z + 1});
            tracker_.notifyBoundaryChange(ChunkCoord{p.x, p.y, p.z - 1});
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
const std::vector<ChunkCoord>& VoxelSimulationSystem::settledChunks() const {
    return settledChunks_;
}
fabric::JobScheduler& VoxelSimulationSystem::scheduler() {
    return scheduler_;
}
ChangeVelocityTracker& VoxelSimulationSystem::velocityTracker() {
    return velocityTracker_;
}
const ChangeVelocityTracker& VoxelSimulationSystem::velocityTracker() const {
    return velocityTracker_;
}

void VoxelSimulationSystem::resetWorldState() {
    ghosts_.clear();
    settledChunks_.clear();
    velocityTracker_.clear();
    frameIndex_ = 0;
    worldSeed_ = 0;
}

void VoxelSimulationSystem::setWorldSeed(int64_t seed) {
    worldSeed_ = seed;
}

} // namespace recurse::simulation
