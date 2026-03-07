#pragma once

#include "fabric/core/Rendering.hh"
#include "fabric/core/Spatial.hh"
#include "recurse/world/ChunkedGrid.hh"

namespace recurse::physics {

using fabric::AABB;
using fabric::Vec3f;

/// Build an AABB from a position (feet origin) and dimensions.
AABB getAABB(const Vec3f& pos, float width, float height, float depth);

/// Check if a single voxel is solid in the density grid.
bool isSolid(int vx, int vy, int vz, const ChunkedGrid<float>& grid, float threshold);

/// Check if an AABB overlaps any solid voxel in the density grid.
/// epsilon is subtracted from max bounds for edge-case floor calculations.
bool aabbOverlapsSolid(const AABB& box, const ChunkedGrid<float>& grid, float threshold, float epsilon);

} // namespace recurse::physics
