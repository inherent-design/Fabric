#pragma once

#include "fabric/core/ChunkedGrid.hh"

#include <algorithm>
#include <cmath>
#include <unordered_set>
#include <vector>

namespace fabric {

struct StreamingConfig {
    int baseRadius = 8;
    int maxRadius = 16;
    float speedScale = 0.5f;
    int maxLoadsPerTick = 4;
    int maxUnloadsPerTick = 4;
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
    explicit ChunkStreamingManager(StreamingConfig config = {});

    StreamingUpdate update(float viewX, float viewY, float viewZ, float speed);

    int currentRadius() const;
    size_t trackedChunkCount() const;
    const StreamingConfig& config() const;

  private:
    StreamingConfig config_;
    int currentRadius_ = 0;
    std::unordered_set<ChunkCoord, ChunkCoordHash> tracked_;
};

} // namespace fabric
