#pragma once

#include <cstdint>

namespace fabric::render {

/// Centralized view ID assignments for the bgfx render pipeline.
/// bgfx sorts draw calls by view ID; lower IDs render first.
namespace view {

inline constexpr uint8_t K_SKY = 0;
inline constexpr uint8_t K_GEOMETRY = 1;
inline constexpr uint8_t K_TRANSPARENT = 2;
inline constexpr uint8_t K_PARTICLES = 10;
inline constexpr uint8_t K_POST_BASE = 200;
inline constexpr uint8_t K_POST_BRIGHT = 200;
inline constexpr uint8_t K_POST_BLUR_1 = 201;
inline constexpr uint8_t K_POST_BLUR_2 = 202;
inline constexpr uint8_t K_POST_BLUR_3 = 203;
inline constexpr uint8_t K_POST_TONEMAP = 204;
inline constexpr uint8_t K_PANINI = 206;
inline constexpr uint8_t K_OIT_ACCUM = 210;
inline constexpr uint8_t K_OIT_COMPOSITE = 211;
inline constexpr uint8_t K_SHADOW_BASE = 240;
inline constexpr uint8_t K_SHADOW_MAX = 243;
inline constexpr uint8_t K_UI = 255;

static_assert(K_PANINI != K_OIT_ACCUM, "Panini and OIT accumulation must not share a view ID");
static_assert(K_POST_TONEMAP < K_PANINI, "Panini must execute after post-process");
static_assert(K_OIT_COMPOSITE < K_SHADOW_BASE, "OIT must complete before shadow passes");

} // namespace view
} // namespace fabric::render
