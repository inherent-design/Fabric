#pragma once

#include "fabric/core/Spatial.hh"
#include <cmath>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace recurse {

// Engine types imported from fabric:: namespace
namespace Space = fabric::Space;
using fabric::Vector4;

/// Maps continuous vec4 essence values to discrete palette indices.
/// Entries within epsilon distance are merged to the same index.
/// When full, the two closest entries are silently merged (D-38).
/// Grid-quantized hash map provides O(1) amortized dedup for epsilon > 0.
class EssencePalette {
  public:
    static constexpr uint16_t K_DEFAULT_MAX_SIZE = 65535;
    static constexpr float K_DEFAULT_EPSILON = 0.01f;

    explicit EssencePalette(float epsilon = K_DEFAULT_EPSILON, uint16_t maxSize = K_DEFAULT_MAX_SIZE);

    /// Map continuous essence to palette index. Adds a new entry if no existing
    /// entry is within epsilon distance. On overflow, silently merges the two
    /// closest entries and returns the merged entry's index for the new value.
    uint16_t quantize(const Vector4<float, Space::World>& essence);

    /// Convenience wrapper: quantize and cast to uint8_t for VoxelCell writers.
    uint8_t quantize8(const Vector4<float, Space::World>& essence);

    /// Reverse lookup: palette index to essence value.
    Vector4<float, Space::World> lookup(uint16_t index) const;

    /// Add an entry explicitly (deduplicates against existing).
    /// Returns the index of the entry (existing or new).
    uint16_t addEntry(const Vector4<float, Space::World>& essence);

    uint16_t addEntryRaw(const Vector4<float, Space::World>& essence);

    size_t paletteSize() const;
    uint16_t maxSize() const;
    float epsilon() const;
    void setEpsilon(float eps);
    void clear();

  private:
    float epsilon_;
    uint16_t maxSize_;
    std::vector<Vector4<float, Space::World>> entries_;

    // Grid hash for O(1) dedup (epsilon > 0). Cell size = epsilon.
    struct GridKey {
        int16_t c[4];
        bool operator==(const GridKey&) const = default;
    };

    struct GridKeyHash {
        size_t operator()(const GridKey& k) const noexcept {
            auto u = [](int16_t v) {
                return static_cast<uint16_t>(v);
            };
            uint64_t h = u(k.c[0]);
            h = h * 65537u + u(k.c[1]);
            h = h * 65537u + u(k.c[2]);
            h = h * 65537u + u(k.c[3]);
            return static_cast<size_t>(h ^ (h >> 16));
        }
    };

    GridKey toGridKey(const Vector4<float, Space::World>& v) const;
    void rebuildGridMap();
    std::unordered_map<GridKey, uint16_t, GridKeyHash> gridMap_;

    /// Merge the two closest entries. Returns the index of the merged entry.
    uint16_t mergeClosestPair();
};

} // namespace recurse
