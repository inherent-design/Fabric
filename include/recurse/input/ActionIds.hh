#pragma once

/// Compile-time action ID constants for input bindings and event listeners.
/// Every action ID used in bindKey() or addEventListener() must be defined here.
/// Bare string literals matching these values are prohibited elsewhere.
namespace recurse::input {

// -- Movement --
inline constexpr const char* K_ACTION_MOVE_FORWARD = "move_forward";
inline constexpr const char* K_ACTION_MOVE_BACKWARD = "move_backward";
inline constexpr const char* K_ACTION_MOVE_LEFT = "move_left";
inline constexpr const char* K_ACTION_MOVE_RIGHT = "move_right";
inline constexpr const char* K_ACTION_MOVE_UP = "move_up";
inline constexpr const char* K_ACTION_MOVE_DOWN = "move_down";
inline constexpr const char* K_ACTION_SPEED_BOOST = "speed_boost";

// -- Time --
inline constexpr const char* K_ACTION_TIME_PAUSE = "time_pause";
inline constexpr const char* K_ACTION_TIME_FASTER = "time_faster";
inline constexpr const char* K_ACTION_TIME_SLOWER = "time_slower";

// -- Character toggles --
inline constexpr const char* K_ACTION_TOGGLE_FLY = "toggle_fly";
inline constexpr const char* K_ACTION_TOGGLE_CAMERA = "toggle_camera";

// -- Debug toggles --
inline constexpr const char* K_ACTION_TOGGLE_DEBUG = "toggle_debug";
inline constexpr const char* K_ACTION_TOGGLE_WIREFRAME = "toggle_wireframe";
inline constexpr const char* K_ACTION_TOGGLE_CHUNK_DEBUG = "toggle_chunk_debug";
inline constexpr const char* K_ACTION_TOGGLE_COLLISION_DEBUG = "toggle_collision_debug";
inline constexpr const char* K_ACTION_TOGGLE_BVH_DEBUG = "toggle_bvh_debug";
inline constexpr const char* K_ACTION_TOGGLE_CHUNK_STATES = "toggle_chunk_states";
inline constexpr const char* K_ACTION_TOGGLE_LOD_STATS = "toggle_lod_stats";
inline constexpr const char* K_ACTION_TOGGLE_CONCURRENCY = "toggle_concurrency";

// -- Panels --
inline constexpr const char* K_ACTION_TOGGLE_CONTENT_BROWSER = "toggle_content_browser";
inline constexpr const char* K_ACTION_TOGGLE_BT_DEBUG = "toggle_bt_debug";
inline constexpr const char* K_ACTION_CYCLE_BT_NPC = "cycle_bt_npc";

} // namespace recurse::input
