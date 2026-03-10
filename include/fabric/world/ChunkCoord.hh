#pragma once
#include <cstdint>
#include <functional>

namespace fabric {

struct ChunkCoord {
    int32_t x{0}, y{0}, z{0};
    bool operator==(const ChunkCoord& o) const { return x == o.x && y == o.y && z == o.z; }
};

struct ChunkCoordHash {
    size_t operator()(const ChunkCoord& c) const {
        size_t h = std::hash<int32_t>{}(c.x);
        h ^= std::hash<int32_t>{}(c.y) + 0x9e3779b9 + (h << 6) + (h >> 2);
        h ^= std::hash<int32_t>{}(c.z) + 0x9e3779b9 + (h << 6) + (h >> 2);
        return h;
    }
};

} // namespace fabric
