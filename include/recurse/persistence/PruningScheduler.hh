#pragma once

#include "recurse/persistence/WorldTransactionStore.hh"
#include <cstdint>

namespace fabric::platform {
class WriterQueue;
}

namespace recurse {

inline constexpr float K_PRUNE_INTERVAL_SECONDS = 3600.0f;
inline constexpr int64_t K_CHANGE_RETENTION_MS = 24LL * 60 * 60 * 1000;
inline constexpr int64_t K_SNAPSHOT_RETENTION_MS = 7LL * 24 * 60 * 60 * 1000;

/// Periodic pruning service for change_log and chunk_snapshot tables.
/// Runs every K_PRUNE_INTERVAL_SECONDS (default 1 hour) and deletes
/// entries older than the configured retention windows.
class PruningScheduler {
  public:
    PruningScheduler(WorldTransactionStore& txStore, fabric::platform::WriterQueue& writerQueue);

    /// Called each frame. Accumulates elapsed time and triggers prune
    /// when interval is reached.
    void update(float dt);

    /// Force an immediate prune pass. Safe to call at any time.
    void pruneNow();

    /// Pruning interval in seconds (default 3600).
    float intervalSeconds = K_PRUNE_INTERVAL_SECONDS;

    /// Retention window for change_log entries in milliseconds (default 24h).
    int64_t changeRetentionMs = K_CHANGE_RETENTION_MS;

    /// Retention window for chunk_snapshot entries in milliseconds (default 7d).
    int64_t snapshotRetentionMs = K_SNAPSHOT_RETENTION_MS;

  private:
    WorldTransactionStore& txStore_;
    fabric::platform::WriterQueue& writerQueue_;
    float elapsed_{0.0f};
};

} // namespace recurse
