#pragma once

#include <cstdint>
#include <string>

struct SDL_Window;

namespace toml {
inline namespace v3 {
class table;
} // namespace v3
} // namespace toml

namespace fabric {

struct WindowDesc {
    std::string title = "Fabric";
    int32_t width = 1280;
    int32_t height = 720;
    int32_t minWidth = 640;
    int32_t minHeight = 480;
    int32_t posX = -1; // -1 = centered on target display
    int32_t posY = -1;
    uint32_t displayIndex = 0; // index into PlatformInfo::displays

    bool vulkan = true;
    bool hidpiEnabled = true;
    bool resizable = true;
    bool borderless = false;
    bool fullscreen = false;
    bool fullscreenDesktop = false;
    bool maximized = false;
    bool hidden = false;

    // Convert to SDL_WindowFlags bitmask
    uint64_t toSDLFlags() const;

    // Apply minimum size constraints to an existing window
    void applyConstraints(SDL_Window* window) const;
};

// Create SDL_Window from descriptor. Returns nullptr on failure (logs error).
SDL_Window* createWindow(const WindowDesc& desc);

// Populate WindowDesc from a TOML [window] table. Missing keys retain defaults.
WindowDesc windowDescFromConfig(const toml::table& table);

} // namespace fabric
