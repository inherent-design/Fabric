#pragma once

#include "fabric/world/ChunkedGrid.hh"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace recurse {

using fabric::ChunkedGrid;

inline constexpr int K_CACHE_SIZE = 34;
inline constexpr int K_CACHE_VOLUME = K_CACHE_SIZE * K_CACHE_SIZE * K_CACHE_SIZE;

class ChunkDensityCache {
  public:
    void build(int cx, int cy, int cz, const ChunkedGrid<float>& density);
    float at(int lx, int ly, int lz) const;
    float sample(float lx, float ly, float lz) const;
    const float* data() const;
    float* data();

  private:
    std::array<float, K_CACHE_VOLUME> data_{};

    static int idx(int lx, int ly, int lz) { return lx + ly * K_CACHE_SIZE + lz * K_CACHE_SIZE * K_CACHE_SIZE; }
};

class ChunkMaterialCache {
  public:
    void build(int cx, int cy, int cz, const ChunkedGrid<uint16_t>& materialGrid);
    uint16_t at(int lx, int ly, int lz) const;
    uint16_t sampleNearest(float lx, float ly, float lz) const;

  private:
    std::array<uint16_t, K_CACHE_VOLUME> data_{};

    static int idx(int lx, int ly, int lz) { return lx + ly * K_CACHE_SIZE + lz * K_CACHE_SIZE * K_CACHE_SIZE; }
};

} // namespace recurse
