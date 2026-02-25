#include "fabric/core/EssencePalette.hh"

#include "fabric/core/Log.hh"
#include "fabric/utils/ErrorHandling.hh"
#include "fabric/utils/Profiler.hh"
#include <cmath>

namespace fabric {

EssencePalette::EssencePalette(float epsilon) : epsilon_(epsilon) {}

uint16_t EssencePalette::quantize(const Vector4<float, Space::World>& essence) {
    FABRIC_ZONE_SCOPED;
    return addEntry(essence);
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

    if (entries_.size() >= kMaxPaletteSize) {
        FABRIC_LOG_WARN("EssencePalette overflow: {} entries at max", entries_.size());
        return kMaxPaletteSize;
    }

    auto idx = static_cast<uint16_t>(entries_.size());
    entries_.push_back(essence);
    return idx;
}

size_t EssencePalette::paletteSize() const {
    return entries_.size();
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

} // namespace fabric
