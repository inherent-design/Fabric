#pragma once

#include "recurse/persistence/ChunkStore.hh"
#include <chrono>
#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
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
        float secondsUntilNextSave = -1.0f;
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

    /// Returns true if a chunk has unload-time persistence pending or in flight.
    bool hasPersistPending(int cx, int cy, int cz) const;

    /// Returns a copy of the pending unload-time blob, if present.
    std::optional<ChunkBlob> copyPersistPendingBlob(int cx, int cy, int cz) const;

    /// Number of chunks currently awaiting save.
    size_t pendingCount() const;

    /// Read-only snapshot for UI and diagnostic save status.
    ActivitySnapshot activitySnapshot() const;

    /// Debounce timing configuration.
    float debounceSeconds = 1.3f;
    float maxDelaySeconds = 5.0f;

  private:
    struct DirtyEntry {
        bool saving{false};
        bool resaveRequested{false};
    };

    struct PreparedEntry {
        fabric::ChunkCoord coord;
        ChunkBlob blob;
        bool saving{false};
        bool resaveRequested{false};
    };

    using ChunkKey = int64_t;
    static ChunkKey makeKey(int cx, int cy, int cz);

    void dispatchBatch(std::vector<std::tuple<int, int, int>> chunks);
    void resetDirtyCadenceLocked();

    ChunkStore& store_;
    fabric::platform::WriterQueue& writerQueue_;
    DataProvider provider_;

    mutable std::mutex mutex_;
    std::unordered_map<ChunkKey, DirtyEntry> dirty_;
    std::unordered_map<ChunkKey, PreparedEntry> prepared_;
    uint64_t nextBatchSerial_ = 0;
    uint64_t lastStartedSerial_ = 0;
    uint64_t lastCompletedSerial_ = 0;
    uint64_t lastSuccessfulSerial_ = 0;
    std::string lastError_;
    float firstDirtyAge_ = 0.0f;
    float lastDirtyAge_ = 0.0f;
    bool dirtyCadenceActive_ = false;
};

} // namespace recurse
