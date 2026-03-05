#pragma once
#include "fabric/simulation/ChunkActivityTracker.hh"
#include "fabric/simulation/GhostCells.hh"
#include "fabric/simulation/MaterialRegistry.hh"
#include "fabric/simulation/SimulationGrid.hh"
#include <random>

namespace fabric::simulation {

class FallingSandSystem {
  public:
    explicit FallingSandSystem(const MaterialRegistry& registry);

    bool simulateGravity(ChunkPos pos, SimulationGrid& grid, const GhostCellManager& ghosts,
                         ChunkActivityTracker& tracker, uint64_t frameIndex, std::mt19937& rng);

    bool simulateLiquid(ChunkPos pos, SimulationGrid& grid, const GhostCellManager& ghosts,
                        ChunkActivityTracker& tracker, uint64_t frameIndex, std::mt19937& rng);

    void simulateChunk(ChunkPos pos, SimulationGrid& grid, const GhostCellManager& ghosts,
                       ChunkActivityTracker& tracker, uint64_t frameIndex, std::mt19937& rng);

  private:
    const MaterialRegistry& registry_;

    VoxelCell readCell(ChunkPos pos, int lx, int ly, int lz, const SimulationGrid& grid,
                       const GhostCellManager& ghosts) const;

    bool canDisplace(VoxelCell mover, VoxelCell target) const;

    void writeSwap(ChunkPos pos, int srcLx, int srcLy, int srcLz, int dstLx, int dstLy, int dstLz, VoxelCell srcCell,
                   VoxelCell dstCell, SimulationGrid& grid, ChunkActivityTracker& tracker) const;
};

} // namespace fabric::simulation
