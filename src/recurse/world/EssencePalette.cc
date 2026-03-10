#include "recurse/world/EssencePalette.hh"

#include "fabric/core/Log.hh"
#include "fabric/utils/ErrorHandling.hh"
#include "fabric/utils/Profiler.hh"
#include <cmath>
#include <limits>

using namespace fabric;

namespace recurse {

EssencePalette::EssencePalette(float epsilon, uint16_t maxSize) : epsilon_(epsilon), maxSize_(maxSize) {}

uint16_t EssencePalette::quantize(const Vector4<float, Space::World>& essence) {
    FABRIC_ZONE_SCOPED;
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
    FABRIC_ZONE_SCOPED;

    float epsSq = epsilon_ * epsilon_;
    for (size_t i = 0; i < entries_.size(); ++i) {
        const auto& e = entries_[i];
        float dx = e.x - essence.x;
        float dy = e.y - essence.y;
        float dz = e.z - essence.z;
        float dw = e.w - essence.w;
        float distSq = dx * dx + dy * dy + dz * dz + dw * dw;
        if (distSq <= epsSq) {
            return static_cast<uint16_t>(i);
        }
    }

    if (entries_.size() >= maxSize_) {
        uint16_t merged = mergeClosestPair();
        // After merge, one slot freed. Try dedup against existing entries again
        // (the new merged entry might now match the incoming essence).
        for (size_t i = 0; i < entries_.size(); ++i) {
            const auto& e = entries_[i];
            float dx = e.x - essence.x;
            float dy = e.y - essence.y;
            float dz = e.z - essence.z;
            float dw = e.w - essence.w;
            float distSq = dx * dx + dy * dy + dz * dz + dw * dw;
            if (distSq <= epsSq) {
                return static_cast<uint16_t>(i);
            }
        }
        // Still no match; append into the freed slot.
        auto idx = static_cast<uint16_t>(entries_.size());
        entries_.push_back(essence);
        return idx;
    }

    auto idx = static_cast<uint16_t>(entries_.size());
    entries_.push_back(essence);
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
            float dx = entries_[i].x - entries_[j].x;
            float dy = entries_[i].y - entries_[j].y;
            float dz = entries_[i].z - entries_[j].z;
            float dw = entries_[i].w - entries_[j].w;
            float distSq = dx * dx + dy * dy + dz * dz + dw * dw;
            if (distSq < bestDistSq) {
                bestDistSq = distSq;
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
}

void EssencePalette::clear() {
    entries_.clear();
}

} // namespace recurse
