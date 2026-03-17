#pragma once
#include "fabric/platform/JobScheduler.hh"
#include "recurse/simulation/BoundaryWriteQueue.hh"
#include "recurse/simulation/ChangeVelocityTracker.hh"
#include "recurse/simulation/ChunkActivityTracker.hh"
#include "recurse/simulation/FallingSandSystem.hh"
#include "recurse/simulation/GhostCells.hh"
#include "recurse/simulation/MaterialRegistry.hh"
#include "recurse/simulation/SimulationGrid.hh"
#include <bit>
#include <cstdint>
#include <vector>

namespace recurse::simulation {

inline uint64_t spatialHash(ChunkCoord pos) {
    uint64_t h = static_cast<uint64_t>(static_cast<uint32_t>(pos.x)) * 73856093ULL;
    h ^= static_cast<uint64_t>(static_cast<uint32_t>(pos.y)) * 19349663ULL;
    h ^= static_cast<uint64_t>(static_cast<uint32_t>(pos.z)) * 83492791ULL;
    return h;
}

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

    void setWorldSeed(int64_t seed);

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
    int64_t worldSeed_ = 0;
    std::vector<ChunkCoord> settledChunks_;
    ChangeVelocityTracker velocityTracker_;

    void propagateDirty(const std::vector<ActiveChunkEntry>& active);
    void drainBoundaryWrites(std::vector<BoundaryWriteQueue>& queues);

  public:
    fabric::JobScheduler& scheduler();

    ChangeVelocityTracker& velocityTracker();
    const ChangeVelocityTracker& velocityTracker() const;
};

} // namespace recurse::simulation
