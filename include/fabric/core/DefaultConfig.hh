#pragma once

#include <cstdint>

namespace fabric {

/// Compiled defaults (Layer 0). Used when no TOML files are found.
/// All fields correspond to fabric.toml schema keys.
namespace DefaultConfig {

// Window (11 fields from platform design, per CONFLICT-5 resolution)
inline constexpr const char* kWindowTitle = "Fabric";
inline constexpr int32_t kWindowWidth = 1280;
inline constexpr int32_t kWindowHeight = 720;
inline constexpr int32_t kMinWindowWidth = 640;
inline constexpr int32_t kMinWindowHeight = 480;
inline constexpr int32_t kDisplay = 0;
inline constexpr bool kFullscreen = false;
inline constexpr bool kBorderless = false;
inline constexpr bool kResizable = true;
inline constexpr bool kHiDPI = true;
inline constexpr bool kMaximized = false;

// Renderer (vsync lives here, not in [window])
inline constexpr const char* kRendererBackend = "vulkan";
inline constexpr bool kRendererDebug = false;
inline constexpr bool kVsync = true;

// Logging
inline constexpr const char* kLogLevel = "info";
inline constexpr bool kFileSink = true;
inline constexpr bool kConsoleSink = true;

// Profiling
inline constexpr bool kProfilingEnabled = false;
inline constexpr const char* kProfilingConnectAddress = "";

// Platform
inline constexpr bool kMimallocOverride = false;

} // namespace DefaultConfig
} // namespace fabric
