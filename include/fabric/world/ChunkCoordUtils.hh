#pragma once

#include <cstdint>
#include <tuple>

namespace fabric {

/// Pack three chunk/voxel coordinates into a single 64-bit key.
/// Uses 21 bits per Y/Z component, remaining upper bits for X.
/// Shared across ChunkedGrid, SimulationGrid, Pathfinding, ReverbZone, StructuralIntegrity.
inline uint64_t packChunkKey(int cx, int cy, int cz) {
    return (static_cast<uint64_t>(cx) << 42) | (static_cast<uint64_t>(cy & 0x1FFFFF)) << 21 |
           static_cast<uint64_t>(cz & 0x1FFFFF);
}

/// Unpack a 64-bit key into three chunk coordinates.
/// Inverse of packChunkKey; sign-extends 21-bit Y/Z components.
inline std::tuple<int, int, int> unpackChunkKey(uint64_t key) {
    int cx = static_cast<int>(key >> 42);
    int cy = static_cast<int>((key >> 21) & 0x1FFFFF);
    int cz = static_cast<int>(key & 0x1FFFFF);
    // Sign-extend all components (X: 22-bit, Y/Z: 21-bit)
    if (cx & 0x200000)
        cx |= ~0x3FFFFF;
    if (cy & 0x100000)
        cy |= ~0x1FFFFF;
    if (cz & 0x100000)
        cz |= ~0x1FFFFF;
    return {cx, cy, cz};
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

} // namespace fabric
