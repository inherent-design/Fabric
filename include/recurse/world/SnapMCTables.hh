#pragma once

#include <cstdint>

namespace recurse {

// Standard Marching Cubes edge table (Paul Bourke / Cory Gene Bloyd).
// 256 entries. Each bit indicates which of the 12 edges are crossed.
extern const uint16_t K_MC_EDGE_TABLE[256];

// Standard Marching Cubes triangle table. 256 entries, up to 5 triangles
// (15 edge indices) per configuration, -1 terminated.
extern const int8_t K_MC_TRI_TABLE[256][16];

// Edge endpoint pairs: which two corners define each of the 12 MC edges.
extern const uint8_t K_EDGE_ENDPOINTS[12][2];

// Corner offsets in (x,y,z) for the 8 cube corners.
// Corner 0: (0,0,0), Corner 1: (1,0,0), Corner 2: (1,1,0), Corner 3: (0,1,0)
// Corner 4: (0,0,1), Corner 5: (1,0,1), Corner 6: (1,1,1), Corner 7: (0,1,1)
extern const uint8_t K_CORNER_OFFSETS[8][3];

} // namespace recurse
