#include "recurse/world/ChunkDensityCache.hh"

using fabric::K_CHUNK_SIZE;

namespace recurse {

// -- ChunkDensityCache --------------------------------------------------------

void ChunkDensityCache::build(int cx, int cy, int cz, const ChunkedGrid<float>& density) {
    int baseX = cx * K_CHUNK_SIZE;
    int baseY = cy * K_CHUNK_SIZE;
    int baseZ = cz * K_CHUNK_SIZE;

    for (int lz = 0; lz < K_CACHE_SIZE; ++lz) {
        for (int ly = 0; ly < K_CACHE_SIZE; ++ly) {
            for (int lx = 0; lx < K_CACHE_SIZE; ++lx) {
                int wx = baseX + (lx - 1);
                int wy = baseY + (ly - 1);
                int wz = baseZ + (lz - 1);
                data_[idx(lx, ly, lz)] = density.get(wx, wy, wz);
            }
        }
    }
}

float ChunkDensityCache::at(int lx, int ly, int lz) const {
    // Clamp to valid range to handle boundary queries when neighbor chunks are missing.
    // This prevents the blur from smoothing solid density toward zero (air) at boundaries.
    lx = std::clamp(lx, 0, K_CACHE_SIZE - 1);
    ly = std::clamp(ly, 0, K_CACHE_SIZE - 1);
    lz = std::clamp(lz, 0, K_CACHE_SIZE - 1);
    return data_[idx(lx, ly, lz)];
}

float ChunkDensityCache::sample(float lx, float ly, float lz) const {
    int x0 = static_cast<int>(std::floor(lx));
    int y0 = static_cast<int>(std::floor(ly));
    int z0 = static_cast<int>(std::floor(lz));

    x0 = std::clamp(x0, 0, K_CACHE_SIZE - 1);
    y0 = std::clamp(y0, 0, K_CACHE_SIZE - 1);
    z0 = std::clamp(z0, 0, K_CACHE_SIZE - 1);

    int x1 = std::min(x0 + 1, K_CACHE_SIZE - 1);
    int y1 = std::min(y0 + 1, K_CACHE_SIZE - 1);
    int z1 = std::min(z0 + 1, K_CACHE_SIZE - 1);

    float fx = lx - static_cast<float>(x0);
    float fy = ly - static_cast<float>(y0);
    float fz = lz - static_cast<float>(z0);

    fx = std::clamp(fx, 0.0f, 1.0f);
    fy = std::clamp(fy, 0.0f, 1.0f);
    fz = std::clamp(fz, 0.0f, 1.0f);

    float c000 = data_[idx(x0, y0, z0)];
    float c100 = data_[idx(x1, y0, z0)];
    float c010 = data_[idx(x0, y1, z0)];
    float c110 = data_[idx(x1, y1, z0)];
    float c001 = data_[idx(x0, y0, z1)];
    float c101 = data_[idx(x1, y0, z1)];
    float c011 = data_[idx(x0, y1, z1)];
    float c111 = data_[idx(x1, y1, z1)];

    float c00 = c000 * (1.0f - fx) + c100 * fx;
    float c10 = c010 * (1.0f - fx) + c110 * fx;
    float c01 = c001 * (1.0f - fx) + c101 * fx;
    float c11 = c011 * (1.0f - fx) + c111 * fx;

    float c0 = c00 * (1.0f - fy) + c10 * fy;
    float c1 = c01 * (1.0f - fy) + c11 * fy;

    return c0 * (1.0f - fz) + c1 * fz;
}

const float* ChunkDensityCache::data() const {
    return data_.data();
}

float* ChunkDensityCache::data() {
    return data_.data();
}

// -- ChunkMaterialCache -------------------------------------------------------

void ChunkMaterialCache::build(int cx, int cy, int cz, const ChunkedGrid<uint16_t>& materialGrid) {
    int baseX = cx * K_CHUNK_SIZE;
    int baseY = cy * K_CHUNK_SIZE;
    int baseZ = cz * K_CHUNK_SIZE;

    for (int lz = 0; lz < K_CACHE_SIZE; ++lz) {
        for (int ly = 0; ly < K_CACHE_SIZE; ++ly) {
            for (int lx = 0; lx < K_CACHE_SIZE; ++lx) {
                int wx = baseX + (lx - 1);
                int wy = baseY + (ly - 1);
                int wz = baseZ + (lz - 1);
                data_[idx(lx, ly, lz)] = materialGrid.get(wx, wy, wz);
            }
        }
    }
}

uint16_t ChunkMaterialCache::at(int lx, int ly, int lz) const {
    // Clamp to valid range for boundary consistency with ChunkDensityCache.
    lx = std::clamp(lx, 0, K_CACHE_SIZE - 1);
    ly = std::clamp(ly, 0, K_CACHE_SIZE - 1);
    lz = std::clamp(lz, 0, K_CACHE_SIZE - 1);
    return data_[idx(lx, ly, lz)];
}

uint16_t ChunkMaterialCache::sampleNearest(float lx, float ly, float lz) const {
    int rx = std::clamp(static_cast<int>(std::round(lx)), 0, K_CACHE_SIZE - 1);
    int ry = std::clamp(static_cast<int>(std::round(ly)), 0, K_CACHE_SIZE - 1);
    int rz = std::clamp(static_cast<int>(std::round(lz)), 0, K_CACHE_SIZE - 1);
    return data_[idx(rx, ry, rz)];
}

} // namespace recurse
