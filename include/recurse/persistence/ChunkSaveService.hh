#pragma once

#include "recurse/persistence/ChunkStore.hh"
#include <chrono>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>

namespace fabric::platform {
class WriterQueue;
}

namespace recurse {

/// Debounced chunk save service. Tracks per-chunk dirty state and
/// batches writes to ChunkStore via WriterQueue serialization.
///
/// Timing: 1.3s debounce (wait for edits to stop), 5s max delay
/// (force save if editing continuously).
class ChunkSaveService {
  public:
    struct ActivitySnapshot {
        size_t dirtyChunks = 0;
        size_t savingChunks = 0;
        size_t preparedChunks = 0;
        uint64_t lastStartedSerial = 0;
        uint64_t lastCompletedSerial = 0;
        uint64_t lastSuccessfulSerial = 0;
        bool hasError = false;
        std::string lastError;
    };

    /// Callback to retrieve current chunk data as FCHK-encoded blob.
    using DataProvider = std::function<ChunkBlob(int cx, int cy, int cz)>;

    ChunkSaveService(ChunkStore& store, fabric::platform::WriterQueue& writerQueue, DataProvider provider);
    ~ChunkSaveService();

    /// Mark a chunk as modified. Resets the debounce timer for that chunk.
    void markDirty(int cx, int cy, int cz);

    /// Called each frame. Checks debounce timers and dispatches saves.
    void update(float dt);

    /// Save all dirty chunks immediately (synchronous). Call on shutdown.
    void flush();

    /// Queue a pre-encoded blob for the next batch save.
    /// Used by the unload loop to avoid touching writerDb_ on the main thread.
    void enqueuePrepared(int cx, int cy, int cz, ChunkBlob blob);

    /// Number of chunks currently awaiting save.
    size_t pendingCount() const;

    /// Read-only snapshot for UI and diagnostic save status.
    ActivitySnapshot activitySnapshot() const;

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

    void dispatchBatch(std::vector<std::tuple<int, int, int>> chunks);

    ChunkStore& store_;
    fabric::platform::WriterQueue& writerQueue_;
    DataProvider provider_;

    mutable std::mutex mutex_;
    std::unordered_map<ChunkKey, DirtyEntry> dirty_;
    std::vector<std::pair<fabric::ChunkCoord, ChunkBlob>> preparedBlobs_;
    uint64_t nextBatchSerial_ = 0;
    uint64_t lastStartedSerial_ = 0;
    uint64_t lastCompletedSerial_ = 0;
    uint64_t lastSuccessfulSerial_ = 0;
    std::string lastError_;
};

} // namespace recurse
