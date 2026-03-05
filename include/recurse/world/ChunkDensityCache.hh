#pragma once

#include "recurse/world/ChunkedGrid.hh"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace recurse {

inline constexpr int kCacheSize = 34;
inline constexpr int kCacheVolume = kCacheSize * kCacheSize * kCacheSize;

class ChunkDensityCache {
  public:
    void build(int cx, int cy, int cz, const ChunkedGrid<float>& density);
    float at(int lx, int ly, int lz) const;
    float sample(float lx, float ly, float lz) const;
    const float* data() const;

  private:
    std::array<float, kCacheVolume> data_{};

    static int idx(int lx, int ly, int lz) { return lx + ly * kCacheSize + lz * kCacheSize * kCacheSize; }
};

class ChunkMaterialCache {
  public:
    void build(int cx, int cy, int cz, const ChunkedGrid<uint16_t>& materialGrid);
    uint16_t at(int lx, int ly, int lz) const;
    uint16_t sampleNearest(float lx, float ly, float lz) const;

  private:
    std::array<uint16_t, kCacheVolume> data_{};

    static int idx(int lx, int ly, int lz) { return lx + ly * kCacheSize + lz * kCacheSize * kCacheSize; }
};

} // namespace recurse
