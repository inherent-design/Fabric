#pragma once

#include "recurse/input/ActionIds.hh"
#include <array>
#include <cstddef>
#include <SDL3/SDL_keycode.h>

namespace recurse::input {

/// Registration entry for a debug toggle binding.
/// Carries all data needed by Recurse.cc (bindKey), DebugOverlaySystem
/// (addEventListener), and HotkeyPanel (display label).
struct DebugToggleEntry {
    const char* actionId;
    SDL_Keycode defaultKey;
    const char* category;
    const char* label;
};

/// Constexpr table of all debug toggle bindings, sorted by key. Single source of truth
/// for Recurse.cc bindKey calls and HotkeyPanel display labels.
inline constexpr std::array K_DEBUG_TOGGLES = {
    DebugToggleEntry{K_ACTION_TOGGLE_CHUNK_STATES, SDLK_F1, "Debug", "Chunk States"},
    DebugToggleEntry{K_ACTION_TOGGLE_LOD_STATS, SDLK_F2, "Debug", "LOD Stats"},
    DebugToggleEntry{K_ACTION_TOGGLE_DEBUG, SDLK_F3, "Debug", "Debug HUD"},
    DebugToggleEntry{K_ACTION_TOGGLE_WIREFRAME, SDLK_F4, "Debug", "Voxel Wireframe"},
    DebugToggleEntry{K_ACTION_TOGGLE_CONCURRENCY, SDLK_F5, "Debug", "Concurrency"},
    DebugToggleEntry{K_ACTION_TOGGLE_BVH_DEBUG, SDLK_F6, "Debug", "BVH Overlay"},
    DebugToggleEntry{K_ACTION_TOGGLE_COLLISION_DEBUG, SDLK_F10, "Debug", "Collision Debug"},
    DebugToggleEntry{K_ACTION_TOGGLE_CHUNK_DEBUG, SDLK_F12, "Debug", "Chunk Debug"},
};

inline constexpr size_t K_DEBUG_TOGGLE_COUNT = K_DEBUG_TOGGLES.size();
static_assert(K_DEBUG_TOGGLE_COUNT == 8);

} // namespace recurse::input
