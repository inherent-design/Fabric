#pragma once
#include <cstdint>
#include <cstring>

namespace recurse::simulation {

using MaterialId = uint16_t;

namespace material_ids {
inline constexpr MaterialId AIR = 0;
inline constexpr MaterialId STONE = 1;
inline constexpr MaterialId DIRT = 2;
inline constexpr MaterialId SAND = 3;
inline constexpr MaterialId WATER = 4;
inline constexpr MaterialId GRAVEL = 5;
inline constexpr MaterialId COUNT = 6;
} // namespace material_ids

enum class MoveType : uint8_t {
    Static = 0, // Stone, Dirt -- does not move
    Powder = 1, // Sand, Gravel -- falls, cascades diagonally
    Liquid = 2, // Water -- falls, flows horizontally
    Gas = 3     // Future: Steam -- rises
};

namespace voxel_flags {
inline constexpr uint8_t NONE = 0;
inline constexpr uint8_t UPDATED = 1 << 0;   // Modified this epoch
inline constexpr uint8_t FREE_FALL = 1 << 1; // In free-fall (optimization)
// Bits 2-7 reserved
} // namespace voxel_flags

/// 4-byte voxel cell. Fits 32768 cells per 32^3 chunk = 128 KB.
struct VoxelCell {
    MaterialId materialId{material_ids::AIR}; // 2 bytes
    uint8_t temperature{128};                 // 1 byte (reserved, unused in v1)
    uint8_t flags{voxel_flags::NONE};         // 1 byte
};
static_assert(sizeof(VoxelCell) == 4, "VoxelCell must be exactly 4 bytes");

/// Shared properties for a material type.
struct MaterialDef {
    MoveType moveType{MoveType::Static};
    uint8_t density{0};        // 0-255, higher = heavier. Displacement ordering.
    uint8_t viscosity{0};      // 0-255, liquid flow resistance
    uint8_t dispersionRate{0}; // cells/tick horizontal flow
    uint32_t baseColor{0};     // RGBA packed

    // Thermal (reserved, zeroed in v1)
    uint16_t meltPoint{0};
    uint16_t boilPoint{0};
    uint8_t thermalConductivity{0};
};

} // namespace recurse::simulation
