#include "recurse/simulation/ChunkActivityTracker.hh"
#include "recurse/simulation/VoxelConstants.hh"
#include <algorithm>
#include <cmath>

namespace recurse::simulation {

void ChunkActivityTracker::setState(ChunkCoord pos, ChunkState state) {
    chunks_[pos].state = state;
}

ChunkState ChunkActivityTracker::getState(ChunkCoord pos) const {
    auto it = chunks_.find(pos);
    if (it == chunks_.end())
        return ChunkState::Sleeping;
    return it->second.state;
}

void ChunkActivityTracker::markSubRegionActive(ChunkCoord pos, int lx, int ly, int lz) {
    int bitIndex = (lx >> 3) + (ly >> 3) * 4 + (lz >> 3) * 16;
    chunks_[pos].subRegionMask |= (uint64_t{1} << bitIndex);
}

uint64_t ChunkActivityTracker::getSubRegionMask(ChunkCoord pos) const {
    auto it = chunks_.find(pos);
    if (it == chunks_.end())
        return 0;
    return it->second.subRegionMask;
}

void ChunkActivityTracker::clearSubRegionMask(ChunkCoord pos) {
    auto it = chunks_.find(pos);
    if (it != chunks_.end())
        it->second.subRegionMask = 0;
}

void ChunkActivityTracker::notifyBoundaryChange(ChunkCoord neighborPos) {
    auto& info = chunks_[neighborPos];
    // Wake sleeping chunks OR mark active chunks for boundary re-mesh
    if (info.state == ChunkState::Sleeping || info.state == ChunkState::Active)
        info.state = ChunkState::BoundaryDirty;
}

void ChunkActivityTracker::setReferencePoint(int wx, int wy, int wz) {
    refX_ = wx;
    refY_ = wy;
    refZ_ = wz;
}

SimPriority ChunkActivityTracker::computePriority(ChunkCoord pos) const {
    // Convert reference point to chunk coordinates (floor division via shift)
    int refCx = refX_ >> K_CHUNK_SHIFT;
    int refCy = refY_ >> K_CHUNK_SHIFT;
    int refCz = refZ_ >> K_CHUNK_SHIFT;

    int dist = std::abs(pos.x - refCx) + std::abs(pos.y - refCy) + std::abs(pos.z - refCz);

    if (dist <= 3)
        return SimPriority::Immediate;
    if (dist <= 8)
        return SimPriority::Normal;
    if (dist <= 16)
        return SimPriority::Background;
    return SimPriority::Hibernating;
}

std::vector<ActiveChunkEntry> ChunkActivityTracker::collectActiveChunks(int budgetCap) const {
    std::vector<ActiveChunkEntry> result;

    for (const auto& [pos, info] : chunks_) {
        if (info.state == ChunkState::Active || info.state == ChunkState::BoundaryDirty) {
            result.push_back({pos, computePriority(pos)});
        }
    }

    std::sort(result.begin(), result.end(), [](const ActiveChunkEntry& a, const ActiveChunkEntry& b) {
        return static_cast<uint8_t>(a.priority) < static_cast<uint8_t>(b.priority);
    });

    if (budgetCap > 0 && static_cast<int>(result.size()) > budgetCap)
        result.resize(budgetCap);

    return result;
}

void ChunkActivityTracker::putToSleep(ChunkCoord pos) {
    auto it = chunks_.find(pos);
    if (it != chunks_.end()) {
        it->second.state = ChunkState::Sleeping;
        it->second.subRegionMask = 0;
    }
}

void ChunkActivityTracker::resolveBoundaryDirty(ChunkCoord pos, bool needsSimulation) {
    auto it = chunks_.find(pos);
    if (it != chunks_.end()) {
        it->second.state = needsSimulation ? ChunkState::Active : ChunkState::Sleeping;
    }
}

void ChunkActivityTracker::remove(ChunkCoord pos) {
    chunks_.erase(pos);
}

void ChunkActivityTracker::clear() {
    chunks_.clear();
}

} // namespace recurse::simulation
