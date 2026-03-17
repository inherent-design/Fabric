#pragma once
#include "fabric/world/ChunkCoord.hh"
#include <cstdint>
#include <span>
#include <unordered_map>
#include <utility>
#include <vector>

namespace recurse::simulation {

using fabric::ChunkCoord;
using fabric::ChunkCoordHash;

struct ChangeVelocityConfig {
    static constexpr uint32_t K_DEFAULT_RING_SIZE = 256;
    static constexpr uint64_t K_DEFAULT_WINDOW_FRAMES = 240; // 4 seconds at 60 TPS
    static constexpr float K_DEFAULT_SETTLING_THRESHOLD = 0.5f;

    uint32_t ringSize = K_DEFAULT_RING_SIZE;
    uint64_t windowFrames = K_DEFAULT_WINDOW_FRAMES;
    float settlingThreshold = K_DEFAULT_SETTLING_THRESHOLD;
};

struct VelocityEntry {
    uint64_t frame;
    uint32_t swapCount;
};

/// Per-chunk fixed-capacity ring buffer of VelocityEntry.
/// Overwrites oldest entry when full. Not thread-safe; designed for
/// single-writer (simulation thread post-merge) with readers synchronized
/// via the epoch boundary (read after tick completes).
class ChunkVelocityRing {
  public:
    explicit ChunkVelocityRing(uint32_t capacity);

    void push(uint64_t frame, uint32_t swapCount);

    uint32_t size() const;

    /// Read entry at logical index [0..size), 0 = oldest.
    const VelocityEntry& at(uint32_t logicalIndex) const;

    /// View of stored entries for debug/visualization.
    /// Returns up to two contiguous segments (ring may wrap).
    /// Caller must not mutate; valid until next push().
    std::pair<std::span<const VelocityEntry>, std::span<const VelocityEntry>> segments() const;

    uint32_t capacity() const;

  private:
    std::vector<VelocityEntry> buf_;
    uint32_t head_ = 0;
    uint32_t count_ = 0;
};

/// Tracks per-chunk change velocity (cell swaps per second) over a sliding
/// frame window. Fed by FallingSandSystem swap counts after the per-worker
/// merge in VoxelSimulationSystem::tick().
///
/// Threading model: single-writer on the simulation thread (post-merge,
/// pre-epoch-advance). Readers (gameplay systems, debug overlay) access
/// after tick() returns, before the next tick() begins. No concurrent
/// read/write.
class ChangeVelocityTracker {
  public:
    explicit ChangeVelocityTracker(ChangeVelocityConfig config = {});

    /// Record swap count for a chunk at the given simulation frame.
    /// Called once per active chunk per tick, after cellSwapsPerWorker merge.
    void record(fabric::ChunkCoord pos, uint32_t swapCount, uint64_t frame);

    /// Current change rate for a chunk: total swaps / window duration (swaps/sec).
    /// Returns 0 if no data or chunk not tracked.
    float velocity(fabric::ChunkCoord pos) const;

    /// True when velocity is positive but at or below the threshold.
    /// A chunk with velocity == 0 (no recent swaps) is already settled,
    /// not "settling." Settling means activity is tapering off.
    bool isSettling(fabric::ChunkCoord pos, float threshold) const;

    /// Convenience overload using configured default threshold.
    bool isSettling(fabric::ChunkCoord pos) const;

    /// Access the raw ring buffer for a chunk. Returns nullptr if not tracked.
    const ChunkVelocityRing* history(fabric::ChunkCoord pos) const;

    /// Remove tracking data for a chunk (call on chunk unload).
    void remove(fabric::ChunkCoord pos);

    /// Clear all tracking data (call on world reset).
    void clear();

    const ChangeVelocityConfig& config() const;

  private:
    ChangeVelocityConfig config_;
    std::unordered_map<fabric::ChunkCoord, ChunkVelocityRing, fabric::ChunkCoordHash> rings_;
};

} // namespace recurse::simulation
