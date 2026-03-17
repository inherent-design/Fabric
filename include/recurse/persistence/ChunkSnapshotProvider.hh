#pragma once

#include "fabric/core/Temporal.hh"
#include <cstdint>

namespace recurse {

class SnapshotScheduler;
class WorldTransactionStore;

namespace persistence {
class ReplayExecutor;
} // namespace persistence

namespace simulation {
class SimulationGrid;
} // namespace simulation

/// Connects fabric::Timeline to chunk-level snapshot and replay operations.
/// On create, forces a flush of all dirty chunk snapshots. On restore, loads
/// chunk snapshots from the DB at the token timestamp and invokes
/// ReplayExecutor to reach the target state.
class ChunkSnapshotProvider : public fabric::SnapshotProvider {
  public:
    ChunkSnapshotProvider(WorldTransactionStore& txStore, SnapshotScheduler& scheduler,
                          simulation::SimulationGrid& grid, persistence::ReplayExecutor& replayExecutor);

    int64_t onCreateSnapshot(double timelineTime) override;
    void onRestoreSnapshot(int64_t snapshotToken) override;
    const char* providerName() const override;

  private:
    WorldTransactionStore& txStore_;
    SnapshotScheduler& scheduler_;
    simulation::SimulationGrid& grid_;
    persistence::ReplayExecutor& replayExecutor_;
};

} // namespace recurse
