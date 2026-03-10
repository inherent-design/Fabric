#pragma once

#include "fabric/world/ChunkedGrid.hh"

#include <algorithm>
#include <cmath>
#include <unordered_set>
#include <vector>

namespace recurse {

using fabric::ChunkedGrid;

struct StreamingConfig {
    int baseRadius = 8;
    int maxLoadsPerTick = 4;
    int maxUnloadsPerTick = 4;
    int maxTrackedChunks = 0; // 0 = unlimited; >0 = hard cap, farthest evicted first
};

struct ChunkCoord {
    int cx, cy, cz;
    bool operator==(const ChunkCoord& o) const = default;
};

struct ChunkCoordHash {
    size_t operator()(const ChunkCoord& c) const {
        auto h1 = std::hash<int>{}(c.cx);
        auto h2 = std::hash<int>{}(c.cy);
        auto h3 = std::hash<int>{}(c.cz);
        return h1 ^ (h2 * 2654435761u) ^ (h3 * 40503u);
    }
};

struct StreamingUpdate {
    std::vector<ChunkCoord> toLoad;
    std::vector<ChunkCoord> toUnload;
};

class ChunkStreamingManager {
  public:
    explicit ChunkStreamingManager(const StreamingConfig& config = {});

    StreamingUpdate update(float viewX, float viewY, float viewZ);

    int currentRadius() const;
    size_t trackedChunkCount() const;
    const StreamingConfig& config() const;

  private:
    StreamingConfig config_;
    int currentRadius_ = 0;
    std::unordered_set<ChunkCoord, ChunkCoordHash> tracked_;
};

} // namespace recurse
