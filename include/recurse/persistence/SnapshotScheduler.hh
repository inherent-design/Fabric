#pragma once

#include "fabric/world/ChunkCoord.hh"
#include "recurse/persistence/ChunkStore.hh"
#include "recurse/persistence/WorldTransactionStore.hh"
#include <functional>
#include <unordered_set>
#include <utility>
#include <vector>

namespace fabric::platform {
class WriterQueue;
}

namespace recurse {

inline constexpr float K_SNAPSHOT_INTERVAL_SECONDS = 300.0f;

/// Periodic chunk snapshot service for rollback anchoring.
/// Tracks modified chunks and snapshots them at fixed intervals
/// via WorldTransactionStore::saveSnapshot().
class SnapshotScheduler {
  public:
    using DataProvider = std::function<ChunkBlob(int cx, int cy, int cz)>;

    SnapshotScheduler(WorldTransactionStore& txStore, fabric::platform::WriterQueue& writerQueue,
                      DataProvider provider);

    /// Mark a chunk as modified since last snapshot.
    void markDirty(int cx, int cy, int cz);

    /// Called each frame. Accumulates elapsed time and triggers snapshot pass
    /// when interval is reached.
    void update(float dt);

    /// Force an immediate snapshot of all dirty chunks. Called on shutdown.
    void flush();

    /// Number of chunks pending snapshot.
    size_t pendingCount() const;

    /// Snapshot interval in seconds (default 300).
    float intervalSeconds = K_SNAPSHOT_INTERVAL_SECONDS;

  private:
    void snapshotAll();

    WorldTransactionStore& txStore_;
    fabric::platform::WriterQueue& writerQueue_;
    DataProvider provider_;
    std::unordered_set<fabric::ChunkCoord, fabric::ChunkCoordHash> dirty_;
    float elapsed_{0.0f};
};

} // namespace recurse
