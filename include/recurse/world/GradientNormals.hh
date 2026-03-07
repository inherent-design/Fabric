#pragma once

#include "fabric/core/Spatial.hh"
#include "recurse/world/ChunkDensityCache.hh"

namespace recurse {

using Vec3f = fabric::Vector3<float, fabric::Space::World>;

inline Vec3f computeNormal(const ChunkDensityCache& cache, float lx, float ly, float lz, float h = 0.5f) {
    float dx = cache.sample(lx + h, ly, lz) - cache.sample(lx - h, ly, lz);
    float dy = cache.sample(lx, ly + h, lz) - cache.sample(lx, ly - h, lz);
    float dz = cache.sample(lx, ly, lz + h) - cache.sample(lx, ly, lz - h);
    return Vec3f{-dx, -dy, -dz}.normalized();
}

} // namespace recurse
