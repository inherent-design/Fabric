#pragma once

#include "fabric/world/ChunkCoord.hh"
#include "recurse/persistence/ChangeSource.hh"
#include "recurse/persistence/ChunkStore.hh"
#include <cstdint>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace recurse {

struct VoxelAddress {
    int cx, cy, cz;
    int vx, vy, vz;
};

struct VoxelChange {
    int64_t timestamp;
    VoxelAddress addr;
    uint32_t oldCell;
    uint32_t newCell;
    int32_t playerId;
    ChangeSource source;
};

struct ChangeQuery {
    std::optional<std::pair<fabric::ChunkCoord, fabric::ChunkCoord>> chunkRange;
    int64_t fromTime{0};
    int64_t toTime{INT64_MAX};
    int32_t playerId{0};
    int64_t limit{10000};
    int64_t offset{0};
};

struct RollbackSpec {
    std::pair<fabric::ChunkCoord, fabric::ChunkCoord> chunkRange;
    int64_t targetTime;
    int32_t playerId{0};
};

/// Abstract interface for per-voxel change logging, snapshots, and rollback.
/// Complements ChunkStore (materialized state) with historical tracking.
class WorldTransactionStore {
  public:
    virtual ~WorldTransactionStore() = default;

    /// Insert a batch of voxel changes into the change log.
    virtual void logChanges(std::span<const VoxelChange> changes) = 0;

    /// Query the change log with optional filters. Returns matching changes ordered by timestamp descending.
    virtual std::vector<VoxelChange> queryChanges(const ChangeQuery& query) = 0;

    /// Count changes matching the given query without returning rows.
    virtual int64_t countChanges(const ChangeQuery& query) = 0;

    /// Save a full chunk snapshot for rollback anchoring.
    virtual void saveSnapshot(int cx, int cy, int cz, const ChunkBlob& data) = 0;

    /// Load the most recent snapshot before a given time. Returns nullopt if none exists.
    virtual std::optional<ChunkBlob> loadSnapshot(int cx, int cy, int cz, int64_t beforeTime) = 0;

    /// Identify chunks affected by a rollback. Returns distinct chunk coordinates.
    /// Cell-level reverse-apply is the caller's responsibility (WT-5).
    virtual std::vector<fabric::ChunkCoord> rollback(const RollbackSpec& spec) = 0;

    /// Delete change log and snapshot entries older than the given thresholds.
    virtual void prune(int64_t retainChangesAfter, int64_t retainSnapshotsAfter) = 0;

    /// Flush any pending buffered writes. Currently a no-op; reserved for future batching.
    virtual void flush() = 0;
};

} // namespace recurse
