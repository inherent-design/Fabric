#pragma once
#include "recurse/simulation/BoundaryWriteQueue.hh"
#include "recurse/simulation/ChunkActivityTracker.hh"
#include "recurse/simulation/GhostCells.hh"
#include "recurse/simulation/MaterialRegistry.hh"
#include "recurse/simulation/SimulationGrid.hh"
#include <random>

namespace recurse::simulation {

class FallingSandSystem {
  public:
    explicit FallingSandSystem(const MaterialRegistry& registry);

    bool simulateGravity(ChunkPos pos, SimulationGrid& grid, const GhostCellManager& ghosts,
                         ChunkActivityTracker& tracker, uint64_t frameIndex, std::mt19937& rng,
                         BoundaryWriteQueue& boundaryWrites);

    bool simulateLiquid(ChunkPos pos, SimulationGrid& grid, const GhostCellManager& ghosts,
                        ChunkActivityTracker& tracker, uint64_t frameIndex, std::mt19937& rng,
                        BoundaryWriteQueue& boundaryWrites);

    /// Simulate one chunk. Returns true if the chunk settled (no movement; put to sleep).
    bool simulateChunk(ChunkPos pos, SimulationGrid& grid, const GhostCellManager& ghosts,
                       ChunkActivityTracker& tracker, uint64_t frameIndex, std::mt19937& rng,
                       BoundaryWriteQueue& boundaryWrites);

  private:
    const MaterialRegistry& registry_;

    VoxelCell readCell(ChunkPos pos, int lx, int ly, int lz, const SimulationGrid& grid,
                       const GhostCellManager& ghosts) const;

    bool canDisplace(VoxelCell mover, VoxelCell target) const;

    void writeSwap(ChunkPos pos, int srcLx, int srcLy, int srcLz, int dstLx, int dstLy, int dstLz, VoxelCell srcCell,
                   VoxelCell dstCell, SimulationGrid& grid, ChunkActivityTracker& tracker,
                   BoundaryWriteQueue& boundaryWrites) const;
};

} // namespace recurse::simulation
