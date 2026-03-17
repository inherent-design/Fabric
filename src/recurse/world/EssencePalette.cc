#include "recurse/world/EssencePalette.hh"

#include "fabric/log/Log.hh"
#include "fabric/utils/ErrorHandling.hh"
#include "fabric/utils/Profiler.hh"
#include <limits>

using namespace fabric;

namespace recurse {

namespace {

inline float distSq4(const Vector4<float, Space::World>& a, const Vector4<float, Space::World>& b) {
    float dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z, dw = a.w - b.w;
    return dx * dx + dy * dy + dz * dz + dw * dw;
}

} // namespace

EssencePalette::EssencePalette(float epsilon, uint16_t maxSize) : epsilon_(epsilon), maxSize_(maxSize) {}

uint16_t EssencePalette::quantize(const Vector4<float, Space::World>& essence) {
    return addEntry(essence);
}

uint8_t EssencePalette::quantize8(const Vector4<float, Space::World>& essence) {
    return static_cast<uint8_t>(quantize(essence));
}

Vector4<float, Space::World> EssencePalette::lookup(uint16_t index) const {
    if (index >= entries_.size()) {
        throwError("EssencePalette::lookup: index out of range");
    }
    return entries_[index];
}

uint16_t EssencePalette::addEntry(const Vector4<float, Space::World>& essence) {
    float epsSq = epsilon_ * epsilon_;

    if (epsilon_ > 0.0f) {
        // O(1) grid hash lookup for the common case.
        auto key = toGridKey(essence);
        auto it = gridMap_.find(key);
        if (it != gridMap_.end() && distSq4(entries_[it->second], essence) <= epsSq) {
            return it->second;
        }

        if (entries_.size() < maxSize_) {
            auto idx = static_cast<uint16_t>(entries_.size());
            entries_.push_back(essence);
            gridMap_[key] = idx;
            return idx;
        }

        // Overflow: merge closest pair, rebuild grid map, retry.
        FABRIC_LOG_DEBUG("EssencePalette merge: {} entries at capacity", maxSize_);
        mergeClosestPair();
        rebuildGridMap();

        it = gridMap_.find(key);
        if (it != gridMap_.end() && distSq4(entries_[it->second], essence) <= epsSq) {
            return it->second;
        }

        auto idx = static_cast<uint16_t>(entries_.size());
        entries_.push_back(essence);
        gridMap_[key] = idx;
        return idx;
    }

    // epsilon == 0: exact-match linear scan (test paths only).
    for (size_t i = 0; i < entries_.size(); ++i) {
        if (distSq4(entries_[i], essence) <= epsSq) {
            return static_cast<uint16_t>(i);
        }
    }

    if (entries_.size() >= maxSize_) {
        mergeClosestPair();
        for (size_t i = 0; i < entries_.size(); ++i) {
            if (distSq4(entries_[i], essence) <= epsSq) {
                return static_cast<uint16_t>(i);
            }
        }
    }

    auto idx = static_cast<uint16_t>(entries_.size());
    entries_.push_back(essence);
    return idx;
}

uint16_t EssencePalette::addEntryRaw(const Vector4<float, Space::World>& essence) {
    auto idx = static_cast<uint16_t>(entries_.size());
    entries_.push_back(essence);
    if (epsilon_ > 0.0f)
        gridMap_[toGridKey(essence)] = idx;
    return idx;
}

uint16_t EssencePalette::mergeClosestPair() {
    FABRIC_ZONE_SCOPED;

    if (entries_.size() < 2)
        return 0;

    size_t bestA = 0;
    size_t bestB = 1;
    float bestDistSq = std::numeric_limits<float>::max();

    for (size_t i = 0; i < entries_.size(); ++i) {
        for (size_t j = i + 1; j < entries_.size(); ++j) {
            float d = distSq4(entries_[i], entries_[j]);
            if (d < bestDistSq) {
                bestDistSq = d;
                bestA = i;
                bestB = j;
            }
        }
    }

    // Merge B into A (midpoint)
    entries_[bestA] = (entries_[bestA] + entries_[bestB]) * 0.5f;

    // Remove B by swapping with the last entry
    if (bestB != entries_.size() - 1)
        entries_[bestB] = entries_.back();
    entries_.pop_back();

    return static_cast<uint16_t>(bestA);
}

EssencePalette::GridKey EssencePalette::toGridKey(const Vector4<float, Space::World>& v) const {
    float inv = 1.0f / epsilon_;
    return {{
        static_cast<int16_t>(std::lroundf(v.x * inv)),
        static_cast<int16_t>(std::lroundf(v.y * inv)),
        static_cast<int16_t>(std::lroundf(v.z * inv)),
        static_cast<int16_t>(std::lroundf(v.w * inv)),
    }};
}

void EssencePalette::rebuildGridMap() {
    gridMap_.clear();
    gridMap_.reserve(entries_.size() * 2);
    for (size_t i = 0; i < entries_.size(); ++i) {
        gridMap_[toGridKey(entries_[i])] = static_cast<uint16_t>(i);
    }
}

size_t EssencePalette::paletteSize() const {
    return entries_.size();
}

uint16_t EssencePalette::maxSize() const {
    return maxSize_;
}

float EssencePalette::epsilon() const {
    return epsilon_;
}

void EssencePalette::setEpsilon(float eps) {
    epsilon_ = eps;
    rebuildGridMap();
}

void EssencePalette::clear() {
    entries_.clear();
    gridMap_.clear();
}

} // namespace recurse
