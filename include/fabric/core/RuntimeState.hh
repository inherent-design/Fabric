#pragma once

#include <cstdint>
#include <string>

namespace fabric {

/// Transient runtime state (Layer T). Not persisted, not configurable via TOML.
/// Read by any system; written by the owning system (e.g., window resize writes pixelWidth).
struct RuntimeState {
    // Window (written by resize handler)
    uint32_t pixelWidth = 0;
    uint32_t pixelHeight = 0;
    float dpiScale = 1.0f;
    bool fullscreen = false;

    // Renderer (written by bgfx callbacks / debug toggles)
    bool debugOverlay = false;
    bool wireframe = false;
    uint32_t resetFlags = 0;

    // App mode (written by AppModeManager observer)
    std::string currentMode = "Game";
    bool simulationPaused = false;
    bool mouseCaptured = true;

    // Performance (written by frame loop)
    float fps = 0.0f;
    float frameTimeMs = 0.0f;
    float gpuTimeMs = 0.0f;
    int drawCallCount = 0;

    // Subsystem counts (written by respective systems)
    int physicsBodyCount = 0;
    int audioVoiceCount = 0;
    int visibleChunks = 0;
    int totalChunks = 0;
    int meshQueueSize = 0;
    float vramUsageMB = 0.0f;
};

} // namespace fabric
