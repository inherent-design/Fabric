#pragma once
#include <array>
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

/// Broad matter mode for a voxel cell.
/// Values 5-7 reserved for future use (Growth, Unstable, etc.).
enum class Phase : uint8_t {
    Empty = 0,
    Solid = 1,
    Powder = 2,
    Liquid = 3,
    Gas = 4
};

enum class MoveType : uint8_t {
    Static = 0, // Stone, Dirt -- does not move
    Powder = 1, // Sand, Gravel -- falls, cascades diagonally
    Liquid = 2, // Water -- falls, flows horizontally
    Gas = 3     // Future: Steam -- rises
};

namespace voxel_flags {
inline constexpr uint8_t NONE = 0;
inline constexpr uint8_t UPDATED = 1 << 0;   // Bit 0 of 5-bit flags field
inline constexpr uint8_t FREE_FALL = 1 << 1; // Bit 1 of 5-bit flags field
// Bits 2-4 reserved
} // namespace voxel_flags

/// 4-byte voxel cell. Fits 32768 cells per 32^3 chunk = 128 KB.
/// Post Wave-4: essence-first layout matching MatterState Shape C.
struct VoxelCell {
    /// Material/essence identity index. During migration, holds MaterialId directly
    /// (cellMaterialId() casts this to MaterialId). Will become a true essence
    /// palette index when the essenceIdx === materialId invariant is broken.
    uint8_t essenceIdx{0};
    uint8_t displacementRank{0}; ///< CA displacement ordering (0-255).
    uint8_t phaseAndFlags{0};    ///< Low 3 bits = Phase, high 5 bits = flags.
    uint8_t spare{0};            ///< Reserved, must be 0 in v1.

    constexpr Phase phase() const { return static_cast<Phase>(phaseAndFlags & 0x07); }
    constexpr void setPhase(Phase p) {
        phaseAndFlags = static_cast<uint8_t>((phaseAndFlags & 0xF8) | (static_cast<uint8_t>(p) & 0x07));
    }
    constexpr uint8_t flags() const { return (phaseAndFlags >> 3) & 0x1F; }
    constexpr void setFlags(uint8_t f) {
        phaseAndFlags = static_cast<uint8_t>((phaseAndFlags & 0x07) | ((f & 0x1F) << 3));
    }
};
static_assert(sizeof(VoxelCell) == 4, "VoxelCell must be exactly 4 bytes");

/// Shared properties for a material type.
struct MaterialDef {
    MoveType moveType{MoveType::Static};
    uint8_t density{0};                       // 0-255, higher = heavier. Displacement ordering.
    uint8_t viscosity{0};                     // 0-255, liquid flow resistance
    uint8_t dispersionRate{0};                // cells/tick horizontal flow
    uint32_t baseColor{0};                    // ARGB packed (0xAARRGGBB)
    float baseEssence[4]{0.f, 0.f, 0.f, 0.f}; // [Order, Chaos, Life, Decay]

    // Thermal (reserved, zeroed in v1)
    uint16_t meltPoint{0};
    uint16_t boilPoint{0};
    uint8_t thermalConductivity{0};
};

inline std::array<float, 4> unpackARGBColor(uint32_t color) {
    float a = static_cast<float>((color >> 24) & 0xFF) / 255.0f;
    float r = static_cast<float>((color >> 16) & 0xFF) / 255.0f;
    float g = static_cast<float>((color >> 8) & 0xFF) / 255.0f;
    float b = static_cast<float>(color & 0xFF) / 255.0f;
    return {r, g, b, a};
}

/// Terrain render contract for the current voxel-first production path.
/// Full-res chunk meshes, optional smooth comparison meshes, and distant LOD
/// sections all derive visible color from MaterialDef::baseColor. Chunk-local
/// essence remains simulation/debug data.
inline std::array<float, 4> terrainAppearanceColor(const MaterialDef& def) {
    return unpackARGBColor(def.baseColor);
}

} // namespace recurse::simulation
