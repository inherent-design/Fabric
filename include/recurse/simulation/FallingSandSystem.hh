#pragma once
#include "recurse/simulation/BoundaryWriteQueue.hh"
#include "recurse/simulation/ChunkActivityTracker.hh"
#include "recurse/simulation/GhostCells.hh"
#include "recurse/simulation/MaterialRegistry.hh"
#include "recurse/simulation/SimulationGrid.hh"
#include <random>
#include <vector>

namespace recurse::simulation {

struct CellSwap;

class FallingSandSystem {
  public:
    explicit FallingSandSystem(const MaterialRegistry& registry);

    bool simulateGravity(ChunkCoord pos, SimulationGrid& grid, const GhostCellManager& ghosts,
                         ChunkActivityTracker& tracker, bool reverseDir, std::mt19937& rng,
                         BoundaryWriteQueue& boundaryWrites, std::vector<CellSwap>& cellSwaps);

    bool simulateLiquid(ChunkCoord pos, SimulationGrid& grid, const GhostCellManager& ghosts,
                        ChunkActivityTracker& tracker, bool reverseDir, std::mt19937& rng,
                        BoundaryWriteQueue& boundaryWrites, std::vector<CellSwap>& cellSwaps);

    /// Simulate one chunk. Returns true if the chunk settled (no movement; put to sleep).
    bool simulateChunk(ChunkCoord pos, SimulationGrid& grid, const GhostCellManager& ghosts,
                       ChunkActivityTracker& tracker, bool reverseDir, std::mt19937& rng,
                       BoundaryWriteQueue& boundaryWrites, std::vector<CellSwap>& cellSwaps);

  private:
    const MaterialRegistry& registry_;

    VoxelCell readCell(ChunkCoord pos, int lx, int ly, int lz, const SimulationGrid& grid,
                       const GhostCellManager& ghosts) const;

    void writeSwap(ChunkCoord pos, int srcLx, int srcLy, int srcLz, int dstLx, int dstLy, int dstLz, VoxelCell srcCell,
                   VoxelCell dstCell, SimulationGrid& grid, ChunkActivityTracker& tracker,
                   BoundaryWriteQueue& boundaryWrites, std::vector<CellSwap>& cellSwaps) const;
};

} // namespace recurse::simulation
