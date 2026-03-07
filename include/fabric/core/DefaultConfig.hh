#pragma once

#include <cstdint>

namespace fabric {

/// Compiled defaults (Layer 0). Used when no TOML files are found.
/// All fields correspond to fabric.toml schema keys.
namespace DefaultConfig {

// Window (11 fields from platform design, per CONFLICT-5 resolution)
inline constexpr const char* K_WINDOW_TITLE = "Fabric";
inline constexpr int32_t K_WINDOW_WIDTH = 1280;
inline constexpr int32_t K_WINDOW_HEIGHT = 720;
inline constexpr int32_t K_MIN_WINDOW_WIDTH = 640;
inline constexpr int32_t K_MIN_WINDOW_HEIGHT = 480;
inline constexpr int32_t K_DISPLAY = 0;
inline constexpr bool K_FULLSCREEN = false;
inline constexpr bool K_BORDERLESS = false;
inline constexpr bool K_RESIZABLE = true;
inline constexpr bool K_HI_DPI = true;
inline constexpr bool K_MAXIMIZED = false;

// Renderer (vsync lives here, not in [window])
inline constexpr const char* K_RENDERER_BACKEND = "vulkan";
inline constexpr bool K_RENDERER_DEBUG = false;
inline constexpr bool K_VSYNC = true;

// Logging
inline constexpr const char* K_LOG_LEVEL = "info";
inline constexpr bool K_FILE_SINK = true;
inline constexpr bool K_CONSOLE_SINK = true;

// Profiling
inline constexpr bool K_PROFILING_ENABLED = false;
inline constexpr const char* K_PROFILING_CONNECT_ADDRESS = "";

// Platform
inline constexpr bool K_MIMALLOC_OVERRIDE = false;

} // namespace DefaultConfig
} // namespace fabric
