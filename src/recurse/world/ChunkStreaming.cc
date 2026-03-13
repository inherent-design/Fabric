#include "recurse/world/ChunkStreaming.hh"
#include "fabric/utils/Profiler.hh"

#include <climits>

using fabric::K_CHUNK_SIZE;

namespace recurse {

ChunkStreamingManager::ChunkStreamingManager(const StreamingConfig& config) : config_(config) {}

StreamingUpdate ChunkStreamingManager::update(float viewX, float viewY, float viewZ) {
    return update({{viewX, viewY, viewZ, config_.baseRadius}});
}

StreamingUpdate ChunkStreamingManager::update(const std::vector<FocalPoint>& sources) {
    FABRIC_ZONE_SCOPED_N("ChunkStreamingManager::update");

    int maxRadius = 0;
    std::unordered_set<ChunkCoord, ChunkCoordHash> desired;

    for (const auto& src : sources) {
        int centerCX = static_cast<int>(std::floor(src.x / static_cast<float>(K_CHUNK_SIZE)));
        int centerCY = static_cast<int>(std::floor(src.y / static_cast<float>(K_CHUNK_SIZE)));
        int centerCZ = static_cast<int>(std::floor(src.z / static_cast<float>(K_CHUNK_SIZE)));
        int r = src.radius;
        if (r > maxRadius)
            maxRadius = r;

        for (int dz = -r; dz <= r; ++dz)
            for (int dy = -r; dy <= r; ++dy)
                for (int dx = -r; dx <= r; ++dx)
                    desired.insert({centerCX + dx, centerCY + dy, centerCZ + dz});
    }

    currentRadius_ = maxRadius;

    auto minDistSq = [&](const ChunkCoord& c) {
        int best = INT_MAX;
        for (const auto& src : sources) {
            int scx = static_cast<int>(std::floor(src.x / static_cast<float>(K_CHUNK_SIZE)));
            int scy = static_cast<int>(std::floor(src.y / static_cast<float>(K_CHUNK_SIZE)));
            int scz = static_cast<int>(std::floor(src.z / static_cast<float>(K_CHUNK_SIZE)));
            int dx = c.x - scx, dy = c.y - scy, dz = c.z - scz;
            int d = dx * dx + dy * dy + dz * dz;
            if (d < best)
                best = d;
        }
        return best;
    };

    std::vector<ChunkCoord> newChunks;
    for (const auto& c : desired) {
        if (!tracked_.contains(c))
            newChunks.push_back(c);
    }

    std::sort(newChunks.begin(), newChunks.end(),
              [&](const ChunkCoord& a, const ChunkCoord& b) { return minDistSq(a) < minDistSq(b); });

    std::vector<ChunkCoord> oldChunks;
    for (const auto& c : tracked_) {
        if (!desired.contains(c))
            oldChunks.push_back(c);
    }

    std::sort(oldChunks.begin(), oldChunks.end(),
              [&](const ChunkCoord& a, const ChunkCoord& b) { return minDistSq(a) > minDistSq(b); });

    StreamingUpdate result;

    int loadCount = config_.maxLoadsPerTick > 0 ? std::min(static_cast<int>(newChunks.size()), config_.maxLoadsPerTick)
                                                : static_cast<int>(newChunks.size());
    for (int i = 0; i < loadCount; ++i) {
        result.toLoad.push_back(newChunks[static_cast<size_t>(i)]);
        tracked_.insert(newChunks[static_cast<size_t>(i)]);
    }

    int unloadCount = config_.maxUnloadsPerTick > 0
                          ? std::min(static_cast<int>(oldChunks.size()), config_.maxUnloadsPerTick)
                          : static_cast<int>(oldChunks.size());
    for (int i = 0; i < unloadCount; ++i) {
        result.toUnload.push_back(oldChunks[static_cast<size_t>(i)]);
        tracked_.erase(oldChunks[static_cast<size_t>(i)]);
    }

    if (config_.maxTrackedChunks > 0 && static_cast<int>(tracked_.size()) > config_.maxTrackedChunks) {
        std::vector<ChunkCoord> allTracked(tracked_.begin(), tracked_.end());
        std::sort(allTracked.begin(), allTracked.end(),
                  [&](const ChunkCoord& a, const ChunkCoord& b) { return minDistSq(a) > minDistSq(b); });

        int excess = static_cast<int>(tracked_.size()) - config_.maxTrackedChunks;
        for (int i = 0; i < excess; ++i) {
            const auto& c = allTracked[static_cast<size_t>(i)];
            bool alreadyQueued = false;
            for (const auto& u : result.toUnload) {
                if (u == c) {
                    alreadyQueued = true;
                    break;
                }
            }
            if (!alreadyQueued) {
                result.toUnload.push_back(c);
                tracked_.erase(c);
            }
        }
    }

    return result;
}

int ChunkStreamingManager::currentRadius() const {
    return currentRadius_;
}

size_t ChunkStreamingManager::trackedChunkCount() const {
    return tracked_.size();
}

const StreamingConfig& ChunkStreamingManager::config() const {
    return config_;
}

void ChunkStreamingManager::clear() {
    tracked_.clear();
    currentRadius_ = 0;
}

void ChunkStreamingManager::untrack(const ChunkCoord& c) {
    tracked_.erase(c);
}

} // namespace recurse
