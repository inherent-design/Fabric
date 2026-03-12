#pragma once

#include "fabric/world/ChunkCoord.hh"
#include "recurse/persistence/WorldTransactionStore.hh"
#include <cstdint>
#include <vector>

namespace recurse {

namespace simulation {
class SimulationGrid;
}

struct RollbackResult {
    std::vector<fabric::ChunkCoord> affectedChunks;
    int64_t changesReverted{0};
};

/// Executes cell-level rollback by reverse-applying changes from the change log.
/// Reads changes from WorldTransactionStore, writes old cell values back to SimulationGrid.
class RollbackExecutor {
  public:
    RollbackExecutor(WorldTransactionStore& txStore, simulation::SimulationGrid& grid);

    /// Full rollback: revert all changes after targetTime in the specified chunk range.
    /// Queries change_log in reverse chronological order and writes oldCell back
    /// to the grid at each voxel position. Returns affected chunks for re-meshing.
    RollbackResult execute(const RollbackSpec& spec);

    /// Player-only rollback: revert only changes by the specified player.
    RollbackResult executePlayerOnly(const RollbackSpec& spec);

  private:
    RollbackResult applyReverse(const std::vector<VoxelChange>& changes);

    WorldTransactionStore& txStore_;
    simulation::SimulationGrid& grid_;
};

} // namespace recurse
