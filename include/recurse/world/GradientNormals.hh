#pragma once

#include "fabric/render/Geometry.hh"
#include "recurse/world/ChunkDensityCache.hh"

namespace recurse {

using fabric::Vec3f;

inline Vec3f computeNormal(const ChunkDensityCache& cache, float lx, float ly, float lz, float h = 0.5f) {
    float dx = cache.sample(lx + h, ly, lz) - cache.sample(lx - h, ly, lz);
    float dy = cache.sample(lx, ly + h, lz) - cache.sample(lx, ly - h, lz);
    float dz = cache.sample(lx, ly, lz + h) - cache.sample(lx, ly, lz - h);
    Vec3f grad{-dx, -dy, -dz};
    float lenSq = grad.lengthSquared();
    if (lenSq < 1e-12f)
        return Vec3f{0.0f, 1.0f, 0.0f};
    return grad * (1.0f / std::sqrt(lenSq));
}

} // namespace recurse
