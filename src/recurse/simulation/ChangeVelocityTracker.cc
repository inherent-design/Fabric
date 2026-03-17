#include "recurse/simulation/ChangeVelocityTracker.hh"
#include <cassert>

namespace recurse::simulation {

// --- ChunkVelocityRing ---

ChunkVelocityRing::ChunkVelocityRing(uint32_t capacity) : buf_(capacity) {
    assert(capacity > 0);
}

void ChunkVelocityRing::push(uint64_t frame, uint32_t swapCount) {
    buf_[head_] = {frame, swapCount};
    head_ = (head_ + 1) % static_cast<uint32_t>(buf_.size());
    if (count_ < buf_.size())
        ++count_;
}

uint32_t ChunkVelocityRing::size() const {
    return count_;
}

const VelocityEntry& ChunkVelocityRing::at(uint32_t logicalIndex) const {
    assert(logicalIndex < count_);
    uint32_t cap = static_cast<uint32_t>(buf_.size());
    uint32_t start = (count_ < cap) ? 0 : head_;
    uint32_t physIdx = (start + logicalIndex) % cap;
    return buf_[physIdx];
}

std::pair<std::span<const VelocityEntry>, std::span<const VelocityEntry>> ChunkVelocityRing::segments() const {
    if (count_ == 0)
        return {{}, {}};
    uint32_t cap = static_cast<uint32_t>(buf_.size());
    if (count_ < cap) {
        return {std::span<const VelocityEntry>(buf_.data(), count_), {}};
    }
    return {std::span<const VelocityEntry>(buf_.data() + head_, cap - head_),
            std::span<const VelocityEntry>(buf_.data(), head_)};
}

uint32_t ChunkVelocityRing::capacity() const {
    return static_cast<uint32_t>(buf_.size());
}

// --- ChangeVelocityTracker ---

ChangeVelocityTracker::ChangeVelocityTracker(ChangeVelocityConfig config) : config_(config) {}

void ChangeVelocityTracker::record(fabric::ChunkCoord pos, uint32_t swapCount, uint64_t frame) {
    auto [it, inserted] = rings_.try_emplace(pos, config_.ringSize);
    it->second.push(frame, swapCount);
}

float ChangeVelocityTracker::velocity(fabric::ChunkCoord pos) const {
    auto it = rings_.find(pos);
    if (it == rings_.end() || it->second.size() == 0)
        return 0.0f;

    const auto& ring = it->second;
    uint32_t n = ring.size();
    if (n == 1)
        return static_cast<float>(ring.at(0).swapCount);

    const VelocityEntry& newest = ring.at(n - 1);
    uint64_t windowStart = (newest.frame >= config_.windowFrames) ? newest.frame - config_.windowFrames : 0;

    uint32_t totalSwaps = 0;
    uint64_t oldestFrame = newest.frame;
    for (uint32_t i = n; i-- > 0;) {
        const auto& e = ring.at(i);
        if (e.frame < windowStart)
            break;
        totalSwaps += e.swapCount;
        oldestFrame = e.frame;
    }

    float dt = static_cast<float>(newest.frame - oldestFrame) / 60.0f;
    if (dt <= 0.0f)
        return static_cast<float>(totalSwaps);

    return static_cast<float>(totalSwaps) / dt;
}

bool ChangeVelocityTracker::isSettling(fabric::ChunkCoord pos, float threshold) const {
    float v = velocity(pos);
    return v > 0.0f && v <= threshold;
}

bool ChangeVelocityTracker::isSettling(fabric::ChunkCoord pos) const {
    return isSettling(pos, config_.settlingThreshold);
}

const ChunkVelocityRing* ChangeVelocityTracker::history(fabric::ChunkCoord pos) const {
    auto it = rings_.find(pos);
    return it != rings_.end() ? &it->second : nullptr;
}

void ChangeVelocityTracker::remove(fabric::ChunkCoord pos) {
    rings_.erase(pos);
}

void ChangeVelocityTracker::clear() {
    rings_.clear();
}

const ChangeVelocityConfig& ChangeVelocityTracker::config() const {
    return config_;
}

} // namespace recurse::simulation
