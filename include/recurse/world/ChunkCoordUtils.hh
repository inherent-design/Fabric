#pragma once

#include <cstdint>

namespace recurse {

/// Pack three chunk/voxel coordinates into a single 64-bit key.
/// Uses 21 bits per Y/Z component, remaining upper bits for X.
/// Shared across ChunkedGrid, Pathfinding, ReverbZone, StructuralIntegrity.
inline uint64_t packChunkKey(int cx, int cy, int cz) {
    return (static_cast<uint64_t>(cx) << 42) | (static_cast<uint64_t>(cy & 0x1FFFFF)) << 21 |
           static_cast<uint64_t>(cz & 0x1FFFFF);
}

/// 6-connected face neighbor offsets: +X, -X, +Y, -Y, +Z, -Z
inline constexpr int K_FACE_NEIGHBORS[6][3] = {
    {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1},
};

/// 10-connected neighbors: 6 face-adjacent + 4 XZ diagonal
inline constexpr int K_FACE_DIAGONAL_NEIGHBORS[10][3] = {
    // Face-adjacent (6)
    {1, 0, 0},
    {-1, 0, 0},
    {0, 1, 0},
    {0, -1, 0},
    {0, 0, 1},
    {0, 0, -1},
    // XZ diagonal (4)
    {1, 0, 1},
    {1, 0, -1},
    {-1, 0, 1},
    {-1, 0, -1},
};

/// 4 horizontal face neighbors: +X, -X, +Z, -Z
inline constexpr int K_HORIZONTAL_NEIGHBORS[4][3] = {
    {1, 0, 0},
    {-1, 0, 0},
    {0, 0, 1},
    {0, 0, -1},
};

} // namespace recurse
