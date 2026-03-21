#pragma once

#include <cstdint>

namespace recurse::simulation {

/// Broad matter mode for a voxel cell.
/// Values 5-7 reserved for future use (Growth, Unstable, etc.).
enum class Phase : uint8_t {
    Empty = 0,
    Solid = 1,
    Powder = 2,
    Liquid = 3,
    Gas = 4
};

/// MatterState v1: 4-byte cell layout for essence-first world authority.
/// Shape C: essenceIdx(8b) + displacementRank(8b) + phase(3b) + flags(5b) + spare(8b).
struct MatterState {
    uint8_t essenceIdx{0};       ///< Palette index into per-chunk EssencePalette.
    uint8_t displacementRank{0}; ///< CA displacement ordering (0-255). Replaces MaterialDef::density.
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

static_assert(sizeof(MatterState) == 4, "MatterState must be exactly 4 bytes");

} // namespace recurse::simulation
