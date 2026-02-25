#pragma once

#include "fabric/core/Spatial.hh"
#include <cstdint>
#include <vector>

namespace fabric {

// Maps continuous vec4 essence values to discrete palette indices for greedy
// meshing. Entries within epsilon distance are merged to the same index.
// Max palette size: 65536 (uint16_t range).
class EssencePalette {
  public:
    static constexpr uint16_t kMaxPaletteSize = 65535;
    static constexpr float kDefaultEpsilon = 0.01f;

    explicit EssencePalette(float epsilon = kDefaultEpsilon);

    // Map continuous essence to palette index. Adds a new entry if no existing
    // entry is within epsilon distance. Returns kMaxPaletteSize on overflow.
    uint16_t quantize(const Vector4<float, Space::World>& essence);

    // Reverse lookup: palette index to essence value.
    Vector4<float, Space::World> lookup(uint16_t index) const;

    // Add an entry explicitly (deduplicates against existing).
    // Returns the index of the entry (existing or new).
    uint16_t addEntry(const Vector4<float, Space::World>& essence);

    size_t paletteSize() const;
    float epsilon() const;
    void setEpsilon(float eps);
    void clear();

  private:
    float epsilon_;
    std::vector<Vector4<float, Space::World>> entries_;
};

} // namespace fabric
