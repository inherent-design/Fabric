#include "fabric/platform/PlatformInfo.hh"

#include "fabric/log/Log.hh"

#include <bgfx/bgfx.h>
#include <SDL3/SDL.h>

#include <cstdlib>

namespace fabric {

namespace {

std::string detectArch() {
#if defined(__aarch64__) || defined(_M_ARM64)
    return "arm64";
#elif defined(__x86_64__) || defined(_M_X64)
    return "x86_64";
#elif defined(__i386__) || defined(_M_IX86)
    return "x86";
#else
    return "unknown";
#endif
}

std::string getEnvOr(const char* name, const std::string& fallback) {
    const char* val = std::getenv(name);
    if (val != nullptr && val[0] != '\0') {
        return val;
    }
    return fallback;
}

} // namespace

std::string vendorNameFromId(uint16_t vendorId) {
    switch (vendorId) {
        case 0x106b:
            return "Apple";
        case 0x10de:
            return "NVIDIA";
        case 0x1002:
            return "AMD";
        case 0x8086:
            return "Intel";
        case 0x13b5:
            return "ARM";
        default:
            return "Unknown";
    }
}

PlatformDirs resolvePlatformDirs() {
    PlatformDirs dirs;

    const char* base = SDL_GetBasePath();
    if (base != nullptr) {
        dirs.basePath = base;
    }

    char* pref = SDL_GetPrefPath("fabric", "fabric");
    if (pref != nullptr) {
        dirs.prefPath = pref;
        SDL_free(pref);
    }

#if defined(__APPLE__)
    std::string homeDir = getEnvOr("HOME", "/tmp");

    // macOS native: ~/Library/Application Support, ~/Library/Caches
    // Override with XDG env vars if explicitly set
    dirs.configDir = getEnvOr("XDG_CONFIG_HOME", homeDir + "/Library/Application Support") + "/fabric";
    dirs.cacheDir = getEnvOr("XDG_CACHE_HOME", homeDir + "/Library/Caches") + "/fabric";
    dirs.dataDir = getEnvOr("XDG_DATA_HOME", homeDir + "/Library/Application Support") + "/fabric";

#elif defined(_WIN32)
    std::string appData = getEnvOr("APPDATA", "");
    std::string localAppData = getEnvOr("LOCALAPPDATA", "");

    dirs.configDir = getEnvOr("XDG_CONFIG_HOME", appData + "\\Fabric") + "/fabric";
    dirs.cacheDir = getEnvOr("XDG_CACHE_HOME", localAppData + "\\Fabric\\Cache") + "/fabric";
    dirs.dataDir = getEnvOr("XDG_DATA_HOME", appData + "\\Fabric") + "/fabric";

    // When using native Windows paths (no XDG override), strip trailing /fabric
    // since the base already includes the app name
    if (std::getenv("XDG_CONFIG_HOME") == nullptr) {
        dirs.configDir = appData + "\\Fabric";
    }
    if (std::getenv("XDG_CACHE_HOME") == nullptr) {
        dirs.cacheDir = localAppData + "\\Fabric\\Cache";
    }
    if (std::getenv("XDG_DATA_HOME") == nullptr) {
        dirs.dataDir = appData + "\\Fabric";
    }

#else
    // Linux/BSD: XDG Base Directory Specification
    std::string homeDir = getEnvOr("HOME", "/tmp");

    dirs.configDir = getEnvOr("XDG_CONFIG_HOME", homeDir + "/.config") + "/fabric";
    dirs.cacheDir = getEnvOr("XDG_CACHE_HOME", homeDir + "/.cache") + "/fabric";
    dirs.dataDir = getEnvOr("XDG_DATA_HOME", homeDir + "/.local/share") + "/fabric";
#endif

    return dirs;
}

void PlatformInfo::populate() {
    os.name = SDL_GetPlatform();
    os.arch = detectArch();
    os.cpuCount = SDL_GetNumLogicalCPUCores();
    os.systemRAM = SDL_GetSystemRAM();

    // SDL version as a portable proxy (true OS version requires
    // platform-specific calls that are not worth the complexity yet)
    int ver = SDL_GetVersion();
    os.version =
        std::to_string(ver / 1000000) + "." + std::to_string((ver / 1000) % 1000) + "." + std::to_string(ver % 1000);

    dirs = resolvePlatformDirs();

    refresh();

    FABRIC_LOG_INFO("Platform: {} SDL {} ({}), {} CPUs, {} MB RAM", os.name, os.version, os.arch, os.cpuCount,
                    os.systemRAM);
    FABRIC_LOG_INFO("Config dir: {}", dirs.configDir);
}

void PlatformInfo::populateGPU() {
    const bgfx::Caps* caps = bgfx::getCaps();
    if (caps == nullptr) {
        FABRIC_LOG_WARN("bgfx caps unavailable, GPU info incomplete");
        return;
    }

    gpu.vendorId = caps->vendorId;
    gpu.deviceId = caps->deviceId;
    gpu.vendorName = vendorNameFromId(caps->vendorId);
    gpu.driverInfo = bgfx::getRendererName(caps->rendererType);
    gpu.deviceName = gpu.driverInfo;

    FABRIC_LOG_INFO("GPU: {} ({}, vendor 0x{:04x}, device 0x{:04x})", gpu.deviceName, gpu.vendorName, gpu.vendorId,
                    gpu.deviceId);
}

void PlatformInfo::refresh() {
    displays.clear();

    int count = 0;
    SDL_DisplayID* displayIds = SDL_GetDisplays(&count);
    SDL_DisplayID primaryId = SDL_GetPrimaryDisplay();

    if (displayIds != nullptr) {
        for (int i = 0; i < count; ++i) {
            DisplayInfo info;
            info.id = displayIds[i];

            const char* name = SDL_GetDisplayName(displayIds[i]);
            if (name != nullptr) {
                info.name = name;
            }

            SDL_Rect bounds;
            if (SDL_GetDisplayBounds(displayIds[i], &bounds)) {
                info.x = bounds.x;
                info.y = bounds.y;
                info.width = bounds.w;
                info.height = bounds.h;
            }

            info.contentScale = SDL_GetDisplayContentScale(displayIds[i]);

            const SDL_DisplayMode* mode = SDL_GetCurrentDisplayMode(displayIds[i]);
            if (mode != nullptr) {
                info.refreshRate = mode->refresh_rate;
            }

            info.primary = (displayIds[i] == primaryId);
            displays.push_back(std::move(info));
        }
        SDL_free(displayIds);
    }

    inputDevices.clear();

    count = 0;
    SDL_KeyboardID* keyboards = SDL_GetKeyboards(&count);
    if (keyboards != nullptr) {
        for (int i = 0; i < count; ++i) {
            InputDeviceInfo dev;
            dev.id = keyboards[i];
            dev.type = InputDeviceType::Keyboard;
            const char* name = SDL_GetKeyboardNameForID(keyboards[i]);
            dev.name = name != nullptr ? name : "Keyboard";
            inputDevices.push_back(std::move(dev));
        }
        SDL_free(keyboards);
    }

    count = 0;
    SDL_MouseID* mice = SDL_GetMice(&count);
    if (mice != nullptr) {
        for (int i = 0; i < count; ++i) {
            InputDeviceInfo dev;
            dev.id = mice[i];
            dev.type = InputDeviceType::Mouse;
            const char* name = SDL_GetMouseNameForID(mice[i]);
            dev.name = name != nullptr ? name : "Mouse";
            inputDevices.push_back(std::move(dev));
        }
        SDL_free(mice);
    }

    count = 0;
    SDL_JoystickID* gamepads = SDL_GetGamepads(&count);
    if (gamepads != nullptr) {
        for (int i = 0; i < count; ++i) {
            InputDeviceInfo dev;
            dev.id = static_cast<uint32_t>(gamepads[i]);
            dev.type = InputDeviceType::Gamepad;
            const char* name = SDL_GetGamepadNameForID(gamepads[i]);
            dev.name = name != nullptr ? name : "Gamepad";
            inputDevices.push_back(std::move(dev));
        }
        SDL_free(gamepads);
    }

    count = 0;
    SDL_TouchID* touches = SDL_GetTouchDevices(&count);
    if (touches != nullptr) {
        for (int i = 0; i < count; ++i) {
            InputDeviceInfo dev;
            dev.id = static_cast<uint32_t>(touches[i]);
            dev.type = InputDeviceType::Touch;
            const char* name = SDL_GetTouchDeviceName(touches[i]);
            dev.name = name != nullptr ? name : "Touch";
            inputDevices.push_back(std::move(dev));
        }
        SDL_free(touches);
    }
}

const DisplayInfo* PlatformInfo::primaryDisplay() const {
    for (const auto& d : displays) {
        if (d.primary) {
            return &d;
        }
    }
    return displays.empty() ? nullptr : &displays[0];
}

uint32_t PlatformInfo::displayCount() const {
    return static_cast<uint32_t>(displays.size());
}

std::vector<InputDeviceInfo> PlatformInfo::devicesOfType(InputDeviceType type) const {
    std::vector<InputDeviceInfo> result;
    for (const auto& dev : inputDevices) {
        if (dev.type == type) {
            result.push_back(dev);
        }
    }
    return result;
}

bool PlatformInfo::hasGamepad() const {
    for (const auto& dev : inputDevices) {
        if (dev.type == InputDeviceType::Gamepad) {
            return true;
        }
    }
    return false;
}

bool PlatformInfo::hasTouch() const {
    for (const auto& dev : inputDevices) {
        if (dev.type == InputDeviceType::Touch) {
            return true;
        }
    }
    return false;
}

} // namespace fabric
