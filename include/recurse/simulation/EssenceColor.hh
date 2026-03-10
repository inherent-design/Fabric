#pragma once

#include "fabric/core/Spatial.hh"
#include <algorithm>
#include <array>

namespace recurse::simulation {

/// Basis colors for the four essence axes (AESTHETIC.md color scheme).
/// Order: blue-grey, Chaos: red-purple, Life: green-brown, Decay: dark brown-yellow.
namespace essence_colors {
inline constexpr std::array<float, 4> K_ORDER = {0.60f, 0.65f, 0.75f, 1.0f};
inline constexpr std::array<float, 4> K_CHAOS = {0.75f, 0.20f, 0.40f, 1.0f};
inline constexpr std::array<float, 4> K_LIFE = {0.30f, 0.65f, 0.20f, 1.0f};
inline constexpr std::array<float, 4> K_DECAY = {0.50f, 0.35f, 0.15f, 1.0f};
} // namespace essence_colors

/// Convert an essence vec4 [Order, Chaos, Life, Decay] to RGBA color via
/// weighted blend of four basis colors. Components that sum to zero produce
/// a neutral grey fallback.
inline std::array<float, 4> essenceToColor(const fabric::Vector4<float, fabric::Space::World>& e) {
    float sum = e.x + e.y + e.z + e.w;
    if (sum < 1e-6f) {
        return {0.5f, 0.5f, 0.5f, 1.0f};
    }

    float inv = 1.0f / sum;
    float wo = e.x * inv;
    float wc = e.y * inv;
    float wl = e.z * inv;
    float wd = e.w * inv;

    using namespace essence_colors;
    return {std::clamp(wo * K_ORDER[0] + wc * K_CHAOS[0] + wl * K_LIFE[0] + wd * K_DECAY[0], 0.0f, 1.0f),
            std::clamp(wo * K_ORDER[1] + wc * K_CHAOS[1] + wl * K_LIFE[1] + wd * K_DECAY[1], 0.0f, 1.0f),
            std::clamp(wo * K_ORDER[2] + wc * K_CHAOS[2] + wl * K_LIFE[2] + wd * K_DECAY[2], 0.0f, 1.0f), 1.0f};
}

} // namespace recurse::simulation
