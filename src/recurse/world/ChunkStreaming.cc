#include "recurse/world/ChunkStreaming.hh"
#include "fabric/utils/Profiler.hh"

using fabric::K_CHUNK_SIZE;

namespace recurse {

ChunkStreamingManager::ChunkStreamingManager(const StreamingConfig& config) : config_(config) {}

StreamingUpdate ChunkStreamingManager::update(float viewX, float viewY, float viewZ) {
    FABRIC_ZONE_SCOPED_N("ChunkStreamingManager::update");

    currentRadius_ = config_.baseRadius;

    int centerCX = static_cast<int>(std::floor(viewX / static_cast<float>(K_CHUNK_SIZE)));
    int centerCY = static_cast<int>(std::floor(viewY / static_cast<float>(K_CHUNK_SIZE)));
    int centerCZ = static_cast<int>(std::floor(viewZ / static_cast<float>(K_CHUNK_SIZE)));

    std::unordered_set<ChunkCoord, ChunkCoordHash> desired;
    for (int dz = -currentRadius_; dz <= currentRadius_; ++dz) {
        for (int dy = -currentRadius_; dy <= currentRadius_; ++dy) {
            for (int dx = -currentRadius_; dx <= currentRadius_; ++dx) {
                desired.insert({centerCX + dx, centerCY + dy, centerCZ + dz});
            }
        }
    }

    // Chunks to load: in desired but not tracked
    std::vector<ChunkCoord> newChunks;
    for (const auto& c : desired) {
        if (!tracked_.contains(c))
            newChunks.push_back(c);
    }

    // Sort by distance to center (nearest first)
    auto distSq = [&](const ChunkCoord& c) {
        int dx = c.cx - centerCX;
        int dy = c.cy - centerCY;
        int dz = c.cz - centerCZ;
        return dx * dx + dy * dy + dz * dz;
    };
    std::sort(newChunks.begin(), newChunks.end(),
              [&](const ChunkCoord& a, const ChunkCoord& b) { return distSq(a) < distSq(b); });

    // Chunks to unload: in tracked but not desired
    std::vector<ChunkCoord> oldChunks;
    for (const auto& c : tracked_) {
        if (!desired.contains(c))
            oldChunks.push_back(c);
    }

    // Sort by distance to center (farthest first)
    std::sort(oldChunks.begin(), oldChunks.end(),
              [&](const ChunkCoord& a, const ChunkCoord& b) { return distSq(a) > distSq(b); });

    StreamingUpdate result;

    int loadCount = std::min(static_cast<int>(newChunks.size()), config_.maxLoadsPerTick);
    for (int i = 0; i < loadCount; ++i) {
        result.toLoad.push_back(newChunks[static_cast<size_t>(i)]);
        tracked_.insert(newChunks[static_cast<size_t>(i)]);
    }

    int unloadCount = std::min(static_cast<int>(oldChunks.size()), config_.maxUnloadsPerTick);
    for (int i = 0; i < unloadCount; ++i) {
        result.toUnload.push_back(oldChunks[static_cast<size_t>(i)]);
        tracked_.erase(oldChunks[static_cast<size_t>(i)]);
    }

    // Enforce hard chunk budget: evict farthest tracked chunks if over cap
    if (config_.maxTrackedChunks > 0 && static_cast<int>(tracked_.size()) > config_.maxTrackedChunks) {
        std::vector<ChunkCoord> allTracked(tracked_.begin(), tracked_.end());
        std::sort(allTracked.begin(), allTracked.end(),
                  [&](const ChunkCoord& a, const ChunkCoord& b) { return distSq(a) > distSq(b); });

        int excess = static_cast<int>(tracked_.size()) - config_.maxTrackedChunks;
        for (int i = 0; i < excess; ++i) {
            const auto& c = allTracked[static_cast<size_t>(i)];
            // Skip if already in toUnload
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

} // namespace recurse
