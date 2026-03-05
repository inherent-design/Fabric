#pragma once
#include "fabric/simulation/ChunkActivityTracker.hh"
#include "fabric/simulation/FallingSandSystem.hh"
#include "fabric/simulation/GhostCells.hh"
#include "fabric/simulation/MaterialRegistry.hh"
#include "fabric/simulation/SimulationGrid.hh"
#include "fabric/simulation/SimWorkerPool.hh"
#include <cstdint>
#include <random>

namespace fabric::simulation {

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

  private:
    MaterialRegistry registry_;
    SimulationGrid grid_;
    ChunkActivityTracker tracker_;
    GhostCellManager ghosts_;
    FallingSandSystem sandSystem_;
    SimWorkerPool workerPool_;
    uint64_t frameIndex_ = 0;
    std::mt19937 rng_{42};

    void propagateDirty(const std::vector<ActiveChunkEntry>& active);

  public:
    /// Access worker pool (e.g. to call disableForTesting())
    SimWorkerPool& workerPool();
};

} // namespace fabric::simulation
