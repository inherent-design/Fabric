#pragma once

#include "fabric/world/ChunkCoord.hh"
#include "recurse/persistence/ChunkStore.hh"
#include <cstdint>
#include <functional>
#include <span>
#include <vector>

namespace recurse {
struct VoxelChange;
class WorldTransactionStore;
} // namespace recurse

namespace recurse::simulation {
class SimulationGrid;
class FallingSandSystem;
class GhostCellManager;
class ChunkActivityTracker;
} // namespace recurse::simulation

namespace recurse::persistence {

/// Controls replay behavior: speed, direction, and rendering.
struct ReplayConfig {
    /// 0 = headless fast-forward (maximum speed).
    /// >0 = forward at rate (1.0 = real-time).
    /// <0 = visual rewind effect (forward replay + reverse presentation).
    float speed = 0.0f;

    /// When true, skip rendering callbacks entirely. Overrides speed
    /// interpretation: any speed value runs at maximum throughput.
    bool headless = true;
};

/// Per-tick state delivered to visual replay observers.
struct ReplayFrame {
    uint64_t tick;
    int64_t worldTimeMs;
    const simulation::SimulationGrid* grid;
};

/// Replay termination status.
enum class ReplayStatus : uint8_t {
    Ok,
    SnapshotMissing,
    SnapshotDecodeFailed,
    Aborted
};

/// Result of a replay operation.
struct ReplayResult {
    ReplayStatus status = ReplayStatus::Ok;
    uint64_t ticksReplayed = 0;
    int64_t finalTimeMs = 0;
    std::vector<fabric::ChunkCoord> affectedChunks;
};

/// Single chunk state within a snapshot set.
struct ChunkSnapshot {
    fabric::ChunkCoord coord;
    ChunkBlob blob;
};

/// Replay starting point: one or more chunk snapshots at a common time.
struct SnapshotSet {
    int64_t timeMs = 0;
    std::vector<ChunkSnapshot> chunks;
};

/// Receives frames during visual (non-headless) replay.
/// Return false to abort replay early.
using ReplayObserver = std::function<bool(const ReplayFrame&)>;

/// Replays simulation from a snapshot plus user edits to produce
/// world state at an arbitrary point in time.
///
/// Two modes: delta (replay N ticks forward) and target (replay to
/// absolute time T). Both load the nearest snapshot before the
/// target window, apply user edits interleaved with simulation ticks,
/// and return the resulting grid state.
///
/// Thread model: runs on the caller's thread. Headless replay blocks
/// until complete. Visual replay calls the observer once per tick;
/// the caller is responsible for frame pacing.
class ReplayExecutor {
  public:
    ReplayExecutor(WorldTransactionStore& txStore, simulation::SimulationGrid& grid,
                   simulation::FallingSandSystem& sandSystem, simulation::GhostCellManager& ghosts,
                   simulation::ChunkActivityTracker& tracker, int64_t worldSeed);

    ReplayResult replayDelta(const SnapshotSet& snapshot, std::span<const VoxelChange> userEdits, uint64_t tickCount,
                             ReplayConfig config = {}, ReplayObserver observer = nullptr);

    ReplayResult replayToTime(const SnapshotSet& snapshot, std::span<const VoxelChange> userEdits, int64_t targetTimeMs,
                              ReplayConfig config = {}, ReplayObserver observer = nullptr);

  private:
    ReplayResult runLoop(const SnapshotSet& snapshot, std::span<const VoxelChange> userEdits, uint64_t tickCount,
                         ReplayConfig config, ReplayObserver observer);

    WorldTransactionStore& txStore_;
    simulation::SimulationGrid& grid_;
    simulation::FallingSandSystem& sandSystem_;
    simulation::GhostCellManager& ghosts_;
    simulation::ChunkActivityTracker& tracker_;
    int64_t worldSeed_;
};

} // namespace recurse::persistence
