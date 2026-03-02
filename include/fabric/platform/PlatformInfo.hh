#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace fabric {

struct OSInfo {
    std::string name;    // "macOS", "Windows", "Linux"
    std::string version; // SDL version string (OS version not portable)
    std::string arch;    // "arm64", "x86_64"
    int cpuCount = 0;    // logical CPU cores
    int systemRAM = 0;   // total system memory in MB
};

struct GPUInfo {
    uint16_t vendorId = 0;
    uint16_t deviceId = 0;
    std::string vendorName; // "Apple", "NVIDIA", "AMD", "Intel"
    std::string deviceName; // renderer name from bgfx
    std::string driverInfo; // backend type string
};

struct DisplayInfo {
    uint32_t id = 0;  // SDL_DisplayID
    std::string name; // SDL_GetDisplayName
    int32_t x = 0;    // bounds origin
    int32_t y = 0;
    int32_t width = 0; // logical size
    int32_t height = 0;
    float contentScale = 1.0f; // DPI scaling factor
    float refreshRate = 0.0f;  // Hz from current display mode
    bool primary = false;
};

enum class InputDeviceType : uint8_t {
    Keyboard,
    Mouse,
    Gamepad,
    Touch,
    Pen
};

struct InputDeviceInfo {
    uint32_t id = 0;
    InputDeviceType type = InputDeviceType::Keyboard;
    std::string name;
};

struct PlatformDirs {
    std::string basePath;  // executable directory
    std::string prefPath;  // SDL_GetPrefPath user-writable dir
    std::string configDir; // platform config (XDG/Library/AppData)
    std::string cacheDir;  // platform cache
    std::string dataDir;   // platform data
};

struct PlatformInfo {
    OSInfo os;
    GPUInfo gpu;
    std::vector<DisplayInfo> displays;
    std::vector<InputDeviceInfo> inputDevices;
    PlatformDirs dirs;

    uint32_t vulkanApiVersion = 0;

    // Accessors
    const DisplayInfo* primaryDisplay() const;
    uint32_t displayCount() const;
    std::vector<InputDeviceInfo> devicesOfType(InputDeviceType type) const;
    bool hasGamepad() const;
    bool hasTouch() const;

    // Phase 1: OS, dirs, displays, input devices (before bgfx::init)
    void populate();

    // Phase 2: GPU identity from bgfx::getCaps() (after bgfx::init)
    void populateGPU();

    // Re-enumerate displays and input devices on hotplug events
    void refresh();
};

// Resolve platform-standard directories with XDG env var overrides.
// Follows native convention per platform, but respects XDG_* if set.
PlatformDirs resolvePlatformDirs();

} // namespace fabric
