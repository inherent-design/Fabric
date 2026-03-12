#pragma once

#include "recurse/persistence/ChunkStore.hh"
#include <chrono>
#include <functional>
#include <mutex>
#include <unordered_map>

namespace fabric {
class JobScheduler;
}

namespace recurse {

/// Debounced chunk save service. Tracks per-chunk dirty state and
/// batches writes to ChunkStore via JobScheduler background jobs.
///
/// Timing: 1.3s debounce (wait for edits to stop), 5s max delay
/// (force save if editing continuously).
class ChunkSaveService {
  public:
    /// Callback to retrieve current chunk data as FCHK-encoded blob.
    using DataProvider = std::function<ChunkBlob(int cx, int cy, int cz)>;

    ChunkSaveService(ChunkStore& store, fabric::JobScheduler& jobs, DataProvider provider);

    /// Mark a chunk as modified. Resets the debounce timer for that chunk.
    void markDirty(int cx, int cy, int cz);

    /// Called each frame. Checks debounce timers and dispatches saves.
    void update(float dt);

    /// Save all dirty chunks immediately (synchronous). Call on shutdown.
    void flush();

    /// Number of chunks currently awaiting save.
    size_t pendingCount() const;

    /// Debounce timing configuration.
    float debounceSeconds = 1.3f;
    float maxDelaySeconds = 5.0f;

  private:
    struct DirtyEntry {
        float firstDirtyAge{0.0f}; // seconds since first modification
        float lastDirtyAge{0.0f};  // seconds since last modification
        bool saving{false};        // currently being written by background job
    };

    using ChunkKey = int64_t;
    static ChunkKey makeKey(int cx, int cy, int cz);

    void saveChunk(int cx, int cy, int cz);
    void saveChunkSync(int cx, int cy, int cz);

    ChunkStore& store_;
    fabric::JobScheduler& jobs_;
    DataProvider provider_;

    mutable std::mutex mutex_;
    std::unordered_map<ChunkKey, DirtyEntry> dirty_;
};

} // namespace recurse
