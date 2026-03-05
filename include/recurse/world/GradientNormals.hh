#pragma once

#include "recurse/world/ChunkDensityCache.hh"

#include <cmath>

namespace recurse {

struct Vec3f {
    float x, y, z;

    Vec3f normalized() const {
        float len = std::sqrt(x * x + y * y + z * z);
        if (len < 1e-8f)
            return {0.0f, 1.0f, 0.0f};
        float inv = 1.0f / len;
        return {x * inv, y * inv, z * inv};
    }
};

inline Vec3f computeNormal(const ChunkDensityCache& cache, float lx, float ly, float lz, float h = 0.5f) {
    float dx = cache.sample(lx + h, ly, lz) - cache.sample(lx - h, ly, lz);
    float dy = cache.sample(lx, ly + h, lz) - cache.sample(lx, ly - h, lz);
    float dz = cache.sample(lx, ly, lz + h) - cache.sample(lx, ly, lz - h);
    return Vec3f{-dx, -dy, -dz}.normalized();
}

} // namespace recurse
