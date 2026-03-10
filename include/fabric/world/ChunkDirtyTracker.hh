#pragma once
#include "fabric/world/ChunkCoord.hh"
#include <cmath>
#include <cstdint>
#include <functional>
#include <unordered_map>
#include <vector>

namespace fabric {

enum class ChunkState : uint8_t {
    Sleeping = 0,     // No activity -- skip simulation
    Active = 1,       // Cells changing -- simulate this chunk
    BoundaryDirty = 2 // Neighbor changed boundary cells -- check once
};

enum class ChunkPriority : uint8_t {
    Immediate = 0,
    Normal = 1,
    Background = 2,
    Hibernating = 3
};

/// Multi-layer dirty flag system for chunk simulation scheduling.
/// Layer 1: Per-chunk state machine (Sleeping/Active/BoundaryDirty)
/// Layer 2: 64-bit sub-chunk activity bitmask (4x4x4 = 64 sub-regions of 8^3)
/// Layer 3: Wake-on-neighbor-change
class ChunkDirtyTracker {
  public:
    void markActive(const ChunkCoord& pos);
    void markSleeping(const ChunkCoord& pos);
    void markBoundaryDirty(const ChunkCoord& pos);

    ChunkState getState(const ChunkCoord& pos) const;

    /// Mark a sub-chunk region dirty. subX/Y/Z in [0,3].
    void setSubChunkDirty(const ChunkCoord& pos, uint8_t subX, uint8_t subY, uint8_t subZ);
    uint64_t getSubChunkMask(const ChunkCoord& pos) const;
    void clearSubChunkMask(const ChunkCoord& pos);

    /// Set player/camera position for priority calculation
    void setReferencePoint(float x, float y, float z);

    /// Return non-sleeping chunks sorted by priority. Cap at maxCount (0 = unlimited).
    std::vector<ChunkCoord> collectActiveChunks(size_t maxCount = 0) const;

    /// Mark 6 face-adjacent chunks as BoundaryDirty
    void wakeNeighbors(const ChunkCoord& pos);

    size_t activeCount() const;
    size_t totalTracked() const;

  private:
    struct ChunkEntry {
        ChunkState state{ChunkState::Sleeping};
        ChunkPriority priority{ChunkPriority::Normal};
        uint64_t subChunkMask{0};
    };

    struct CoordHash {
        size_t operator()(const ChunkCoord& c) const {
            size_t h = std::hash<int32_t>{}(c.x);
            h ^= std::hash<int32_t>{}(c.y) + 0x9e3779b9 + (h << 6) + (h >> 2);
            h ^= std::hash<int32_t>{}(c.z) + 0x9e3779b9 + (h << 6) + (h >> 2);
            return h;
        }
    };

    std::unordered_map<ChunkCoord, ChunkEntry, CoordHash> entries_;
    float refX_{0}, refY_{0}, refZ_{0};

    ChunkPriority computePriority(const ChunkCoord& pos) const;
};

} // namespace fabric
