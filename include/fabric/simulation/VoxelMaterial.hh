#pragma once
#include <cstdint>
#include <cstring>

namespace fabric::simulation {

using MaterialId = uint16_t;

namespace MaterialIds {
inline constexpr MaterialId Air = 0;
inline constexpr MaterialId Stone = 1;
inline constexpr MaterialId Dirt = 2;
inline constexpr MaterialId Sand = 3;
inline constexpr MaterialId Water = 4;
inline constexpr MaterialId Gravel = 5;
inline constexpr MaterialId Count = 6;
} // namespace MaterialIds

enum class MoveType : uint8_t {
    Static = 0, // Stone, Dirt -- does not move
    Powder = 1, // Sand, Gravel -- falls, cascades diagonally
    Liquid = 2, // Water -- falls, flows horizontally
    Gas = 3     // Future: Steam -- rises
};

namespace VoxelFlags {
inline constexpr uint8_t None = 0;
inline constexpr uint8_t Updated = 1 << 0;  // Modified this epoch
inline constexpr uint8_t FreeFall = 1 << 1; // In free-fall (optimization)
// Bits 2-7 reserved
} // namespace VoxelFlags

/// 4-byte voxel cell. Fits 32768 cells per 32^3 chunk = 128 KB.
struct VoxelCell {
    MaterialId materialId{MaterialIds::Air}; // 2 bytes
    uint8_t temperature{128};                // 1 byte (reserved, unused in v1)
    uint8_t flags{VoxelFlags::None};         // 1 byte
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

} // namespace fabric::simulation
