#include "fabric/core/ChunkDirtyTracker.hh"
#include <algorithm>

namespace fabric {

void ChunkDirtyTracker::markActive(const ChunkCoord& pos) {
    auto& entry = entries_[pos];
    entry.state = ChunkState::Active;
    entry.priority = computePriority(pos);
}

void ChunkDirtyTracker::markSleeping(const ChunkCoord& pos) {
    auto it = entries_.find(pos);
    if (it != entries_.end()) {
        it->second.state = ChunkState::Sleeping;
        it->second.subChunkMask = 0;
    }
}

void ChunkDirtyTracker::markBoundaryDirty(const ChunkCoord& pos) {
    auto it = entries_.find(pos);
    if (it != entries_.end()) {
        // Only upgrade from Sleeping; Active stays Active
        if (it->second.state == ChunkState::Sleeping) {
            it->second.state = ChunkState::BoundaryDirty;
            it->second.priority = computePriority(pos);
        }
    } else {
        // Untracked chunk: create as BoundaryDirty
        auto& entry = entries_[pos];
        entry.state = ChunkState::BoundaryDirty;
        entry.priority = computePriority(pos);
    }
}

ChunkState ChunkDirtyTracker::getState(const ChunkCoord& pos) const {
    auto it = entries_.find(pos);
    if (it == entries_.end()) {
        return ChunkState::Sleeping;
    }
    return it->second.state;
}

void ChunkDirtyTracker::setSubChunkDirty(const ChunkCoord& pos, uint8_t subX, uint8_t subY, uint8_t subZ) {
    uint8_t bit = subZ * 16 + subY * 4 + subX;
    entries_[pos].subChunkMask |= (uint64_t{1} << bit);
}

uint64_t ChunkDirtyTracker::getSubChunkMask(const ChunkCoord& pos) const {
    auto it = entries_.find(pos);
    if (it == entries_.end()) {
        return 0;
    }
    return it->second.subChunkMask;
}

void ChunkDirtyTracker::clearSubChunkMask(const ChunkCoord& pos) {
    auto it = entries_.find(pos);
    if (it != entries_.end()) {
        it->second.subChunkMask = 0;
    }
}

void ChunkDirtyTracker::setReferencePoint(float x, float y, float z) {
    refX_ = x;
    refY_ = y;
    refZ_ = z;
}

std::vector<ChunkCoord> ChunkDirtyTracker::collectActiveChunks(size_t maxCount) const {
    std::vector<std::pair<ChunkPriority, ChunkCoord>> active;
    for (const auto& [coord, entry] : entries_) {
        if (entry.state != ChunkState::Sleeping) {
            active.emplace_back(entry.priority, coord);
        }
    }

    std::sort(active.begin(), active.end(), [](const auto& a, const auto& b) {
        return static_cast<uint8_t>(a.first) < static_cast<uint8_t>(b.first);
    });

    if (maxCount > 0 && active.size() > maxCount) {
        active.resize(maxCount);
    }

    std::vector<ChunkCoord> result;
    result.reserve(active.size());
    for (const auto& [pri, coord] : active) {
        result.push_back(coord);
    }
    return result;
}

void ChunkDirtyTracker::wakeNeighbors(const ChunkCoord& pos) {
    markBoundaryDirty({pos.x + 1, pos.y, pos.z});
    markBoundaryDirty({pos.x - 1, pos.y, pos.z});
    markBoundaryDirty({pos.x, pos.y + 1, pos.z});
    markBoundaryDirty({pos.x, pos.y - 1, pos.z});
    markBoundaryDirty({pos.x, pos.y, pos.z + 1});
    markBoundaryDirty({pos.x, pos.y, pos.z - 1});
}

size_t ChunkDirtyTracker::activeCount() const {
    size_t count = 0;
    for (const auto& [coord, entry] : entries_) {
        if (entry.state != ChunkState::Sleeping) {
            ++count;
        }
    }
    return count;
}

size_t ChunkDirtyTracker::totalTracked() const {
    return entries_.size();
}

ChunkPriority ChunkDirtyTracker::computePriority(const ChunkCoord& pos) const {
    float dx = static_cast<float>(pos.x) - refX_;
    float dy = static_cast<float>(pos.y) - refY_;
    float dz = static_cast<float>(pos.z) - refZ_;
    float dist = std::abs(dx) + std::abs(dy) + std::abs(dz);

    if (dist < 3.0f)
        return ChunkPriority::Immediate;
    if (dist < 7.0f)
        return ChunkPriority::Normal;
    if (dist < 12.0f)
        return ChunkPriority::Background;
    return ChunkPriority::Hibernating;
}

} // namespace fabric
