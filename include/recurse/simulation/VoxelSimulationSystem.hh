#pragma once
#include "fabric/platform/JobScheduler.hh"
#include "recurse/simulation/BoundaryWriteQueue.hh"
#include "recurse/simulation/ChunkActivityTracker.hh"
#include "recurse/simulation/FallingSandSystem.hh"
#include "recurse/simulation/GhostCells.hh"
#include "recurse/simulation/MaterialRegistry.hh"
#include "recurse/simulation/SimulationGrid.hh"
#include <bit>
#include <cstdint>
#include <random>
#include <unordered_map>
#include <vector>

namespace recurse::simulation {

/// Per-cell change record from physics simulation (FallingSand writeSwap).
/// Simulation-layer only; converted to VoxelChangeDetail at system boundary.
struct CellSwap {
    ChunkCoord chunk;
    int lx, ly, lz;
    uint32_t oldCell;
    uint32_t newCell;
};

/// Standalone orchestration loop for voxel simulation.
/// Owns all simulation subsystems and drives the epoch-based tick cycle:
/// collectActive -> syncGhostCells -> simulate -> advanceEpoch -> propagateDirty
class VoxelSimulationSystem {
  public:
    VoxelSimulationSystem();

    /// Run one full simulation epoch.
    void tick();

    SimulationGrid& grid();
    const SimulationGrid& grid() const;
    MaterialRegistry& materials();
    const MaterialRegistry& materials() const;
    ChunkActivityTracker& activityTracker();
    const ChunkActivityTracker& activityTracker() const;
    uint64_t frameIndex() const;

    /// Reset all per-world simulation state.
    /// Clears ghost cells, physics change records, settled list,
    /// and resets the frame counter. Grid and tracker are left to
    /// the caller (outer system clears them separately).
    void resetWorldState();

    /// Chunks that settled (no movement) during the last tick().
    /// Used by the outer system to dispatch collision rebuild events.
    const std::vector<ChunkCoord>& settledChunks() const;

  private:
    MaterialRegistry registry_;
    SimulationGrid grid_;
    ChunkActivityTracker tracker_;
    GhostCellManager ghosts_;
    FallingSandSystem sandSystem_;
    fabric::JobScheduler scheduler_;
    uint64_t frameIndex_ = 0;
    std::mt19937 rng_{42};
    std::vector<ChunkCoord> settledChunks_;
    std::unordered_map<ChunkCoord, std::vector<CellSwap>, ChunkCoordHash> physicsChanges_;

    void propagateDirty(const std::vector<ActiveChunkEntry>& active);
    void drainBoundaryWrites(std::vector<BoundaryWriteQueue>& queues);

  public:
    fabric::JobScheduler& scheduler();

    /// Per-chunk cell swaps from the last tick(). Used by the outer system
    /// to emit detailed K_VOXEL_CHANGED_EVENT for rollback logging.
    const auto& physicsChanges() const { return physicsChanges_; }
};

} // namespace recurse::simulation
