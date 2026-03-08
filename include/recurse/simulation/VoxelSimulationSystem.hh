#pragma once
#include "fabric/core/JobScheduler.hh"
#include "recurse/simulation/ChunkActivityTracker.hh"
#include "recurse/simulation/FallingSandSystem.hh"
#include "recurse/simulation/GhostCells.hh"
#include "recurse/simulation/MaterialRegistry.hh"
#include "recurse/simulation/SimulationGrid.hh"
#include <cstdint>
#include <random>

namespace recurse::simulation {

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
    fabric::JobScheduler scheduler_;
    uint64_t frameIndex_ = 0;
    std::mt19937 rng_{42};

    void propagateDirty(const std::vector<ActiveChunkEntry>& active);

  public:
    fabric::JobScheduler& scheduler();
};

} // namespace recurse::simulation
