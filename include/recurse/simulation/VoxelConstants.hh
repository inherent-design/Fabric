#pragma once

#include "fabric/world/ChunkedGrid.hh"

namespace recurse::simulation {

inline constexpr int K_CHUNK_SIZE = 32;
inline constexpr int K_CHUNK_SHIFT = 5;
inline constexpr int K_CHUNK_MASK = K_CHUNK_SIZE - 1;
inline constexpr int K_CHUNK_VOLUME = K_CHUNK_SIZE * K_CHUNK_SIZE * K_CHUNK_SIZE;

inline constexpr int K_PHYS_TILE_SIZE = 8;
inline constexpr int K_TILES_PER_AXIS = K_CHUNK_SIZE / K_PHYS_TILE_SIZE;
inline constexpr int K_TILES_PER_CHUNK = K_TILES_PER_AXIS * K_TILES_PER_AXIS * K_TILES_PER_AXIS;

} // namespace recurse::simulation
