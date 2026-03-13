#pragma once

#include "fabric/world/ChunkCoord.hh"
#include "fabric/world/ChunkedGrid.hh"

#include <algorithm>
#include <cmath>
#include <unordered_set>
#include <vector>

namespace recurse {

using fabric::ChunkCoord;
using fabric::ChunkCoordHash;
using fabric::ChunkedGrid;

struct StreamingConfig {
    int baseRadius = 8;
    int maxLoadsPerTick = 4;
    int maxUnloadsPerTick = 4;
    int maxTrackedChunks = 0;
};

struct StreamingUpdate {
    std::vector<ChunkCoord> toLoad;
    std::vector<ChunkCoord> toUnload;
};

struct FocalPoint {
    float x, y, z;
    int radius;
};

class ChunkStreamingManager {
  public:
    explicit ChunkStreamingManager(const StreamingConfig& config = {});

    StreamingUpdate update(const std::vector<FocalPoint>& sources);
    StreamingUpdate update(float viewX, float viewY, float viewZ);

    int currentRadius() const;
    size_t trackedChunkCount() const;
    const StreamingConfig& config() const;

    /// Drop all tracked state. Call between world loads.
    void clear();

    /// Remove a single chunk from tracked state so it re-enters toLoad next frame.
    void untrack(const ChunkCoord& c);

  private:
    StreamingConfig config_;
    int currentRadius_ = 0;
    std::unordered_set<ChunkCoord, ChunkCoordHash> tracked_;
};

} // namespace recurse
