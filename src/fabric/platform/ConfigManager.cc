#include "fabric/platform/ConfigManager.hh"

#include <cstdlib>
#include <fstream>
#include <string>

#include <SDL3/SDL.h>

#include "fabric/log/Log.hh"
#include "fabric/platform/DefaultConfig.hh"

namespace fabric {

// -- Construction --

ConfigManager::ConfigManager() : userConfigPath_(defaultUserConfigPath()) {
    // Build Layer 0 from compiled defaults
    toml::table window;
    window.insert_or_assign("title", DefaultConfig::K_WINDOW_TITLE);
    window.insert_or_assign("width", static_cast<int64_t>(DefaultConfig::K_WINDOW_WIDTH));
    window.insert_or_assign("height", static_cast<int64_t>(DefaultConfig::K_WINDOW_HEIGHT));
    window.insert_or_assign("min_width", static_cast<int64_t>(DefaultConfig::K_MIN_WINDOW_WIDTH));
    window.insert_or_assign("min_height", static_cast<int64_t>(DefaultConfig::K_MIN_WINDOW_HEIGHT));
    window.insert_or_assign("display", static_cast<int64_t>(DefaultConfig::K_DISPLAY));
    window.insert_or_assign("fullscreen", DefaultConfig::K_FULLSCREEN);
    window.insert_or_assign("borderless", DefaultConfig::K_BORDERLESS);
    window.insert_or_assign("resizable", DefaultConfig::K_RESIZABLE);
    window.insert_or_assign("hidpi", DefaultConfig::K_HI_DPI);
    window.insert_or_assign("maximized", DefaultConfig::K_MAXIMIZED);

    toml::table renderer;
    renderer.insert_or_assign("backend", DefaultConfig::K_RENDERER_BACKEND);
    renderer.insert_or_assign("debug", DefaultConfig::K_RENDERER_DEBUG);
    renderer.insert_or_assign("vsync", DefaultConfig::K_VSYNC);

    toml::table logging;
    logging.insert_or_assign("level", DefaultConfig::K_LOG_LEVEL);
    logging.insert_or_assign("file_sink", DefaultConfig::K_FILE_SINK);
    logging.insert_or_assign("console_sink", DefaultConfig::K_CONSOLE_SINK);

    toml::table profiling;
    profiling.insert_or_assign("enabled", DefaultConfig::K_PROFILING_ENABLED);
    profiling.insert_or_assign("connect_address", DefaultConfig::K_PROFILING_CONNECT_ADDRESS);

    toml::table platform;
    platform.insert_or_assign("mimalloc_override", DefaultConfig::K_MIMALLOC_OVERRIDE);

    defaults_.insert_or_assign("window", std::move(window));
    defaults_.insert_or_assign("renderer", std::move(renderer));
    defaults_.insert_or_assign("logging", std::move(logging));
    defaults_.insert_or_assign("profiling", std::move(profiling));
    defaults_.insert_or_assign("platform", std::move(platform));

    rebuildMerged();
}

// -- Layer loading --

void ConfigManager::loadDefaults(const toml::table& defaults) {
    std::lock_guard lock(mutex_);
    deepMerge(defaults_, defaults);
    rebuildMerged();
}

void ConfigManager::loadEngineConfig(const std::filesystem::path& path) {
    std::lock_guard lock(mutex_);
    if (!std::filesystem::exists(path)) {
        FABRIC_LOG_WARN("Engine config not found: {}", path.string());
        return;
    }
    try {
        engineLayer_ = toml::parse_file(path.string());
        FABRIC_LOG_INFO("Loaded engine config: {}", path.string());
    } catch (const toml::parse_error& err) {
        FABRIC_LOG_ERROR("Failed to parse engine config {}: {}", path.string(), err.description());
    }
    rebuildMerged();
}

void ConfigManager::loadAppConfig(const std::filesystem::path& path) {
    std::lock_guard lock(mutex_);
    if (!std::filesystem::exists(path)) {
        FABRIC_LOG_WARN("App config not found: {}", path.string());
        return;
    }
    try {
        appLayer_ = toml::parse_file(path.string());
        FABRIC_LOG_INFO("Loaded app config: {}", path.string());
    } catch (const toml::parse_error& err) {
        FABRIC_LOG_ERROR("Failed to parse app config {}: {}", path.string(), err.description());
    }
    rebuildMerged();
}

void ConfigManager::loadUserConfig(const std::filesystem::path& path) {
    std::lock_guard lock(mutex_);
    userConfigPath_ = path;
    if (!std::filesystem::exists(path)) {
        FABRIC_LOG_DEBUG("User config not found (will create on first write): {}", path.string());
        return;
    }
    try {
        userLayer_ = toml::parse_file(path.string());
        FABRIC_LOG_INFO("Loaded user config: {}", path.string());
    } catch (const toml::parse_error& err) {
        FABRIC_LOG_ERROR("Failed to parse user config {}: {}", path.string(), err.description());
    }
    rebuildMerged();
}

// -- CLI overrides --

void ConfigManager::applyCLIOverrides(int argc, char* argv[]) {
    std::lock_guard lock(mutex_);
    for (int i = 1; i < argc; ++i) {
        std::string_view arg(argv[i]);
        if (!arg.starts_with("--"))
            continue;
        arg.remove_prefix(2);

        auto eq = arg.find('=');
        if (eq == std::string_view::npos)
            continue;

        auto key = arg.substr(0, eq);
        std::string val(arg.substr(eq + 1));

        // Special handling: --feature.xxx=value -> features.xxx
        std::string resolvedKey;
        if (key.starts_with("feature.")) {
            resolvedKey = "features." + std::string(key.substr(8)); // feature.audio -> features.audio
        } else {
            resolvedKey = std::string(key);
        }

        if (val == "true") {
            insertValueAtPath(cliLayer_, resolvedKey, true);
        } else if (val == "false") {
            insertValueAtPath(cliLayer_, resolvedKey, false);
        } else {
            // Try integer (no decimal point)
            if (val.find('.') == std::string::npos) {
                try {
                    size_t pos = 0;
                    int64_t intVal = std::stoll(val, &pos);
                    if (pos == val.size()) {
                        insertValueAtPath(cliLayer_, resolvedKey, intVal);
                        continue;
                    }
                } catch (...) {
                    // Fall through to float/string
                }
            }
            // Try float
            try {
                size_t pos = 0;
                double floatVal = std::stod(val, &pos);
                if (pos == val.size()) {
                    insertValueAtPath(cliLayer_, resolvedKey, floatVal);
                    continue;
                }
            } catch (...) {
                // Fall through to string
            }
            // String fallback
            insertValueAtPath(cliLayer_, resolvedKey, val);
        }
    }
    rebuildMerged();
}

// -- Read access --

bool ConfigManager::has(std::string_view key) const {
    std::lock_guard lock(mutex_);
    return static_cast<bool>(merged_.at_path(key));
}

const toml::table& ConfigManager::merged() const {
    return merged_;
}

// -- User layer writes --

void ConfigManager::setUserConfigPath(const std::filesystem::path& path) {
    std::lock_guard lock(mutex_);
    userConfigPath_ = path;
}

void ConfigManager::flushIfDirty() {
    std::lock_guard lock(mutex_);
    if (!userDirty_)
        return;
    auto now = std::chrono::steady_clock::now();
    if (now - lastDirtyTime_ < K_DEBOUNCE_MS)
        return;

    auto dir = userConfigPath_.parent_path();
    if (!dir.empty()) {
        std::filesystem::create_directories(dir);
    }
    std::ofstream ofs(userConfigPath_);
    if (ofs.is_open()) {
        ofs << userLayer_;
        userDirty_ = false;
        FABRIC_LOG_DEBUG("Flushed user config to {}", userConfigPath_.string());
    } else {
        FABRIC_LOG_ERROR("Failed to write user config: {}", userConfigPath_.string());
    }
}

void ConfigManager::flushNow() {
    std::lock_guard lock(mutex_);
    if (!userDirty_)
        return;

    auto dir = userConfigPath_.parent_path();
    if (!dir.empty()) {
        std::filesystem::create_directories(dir);
    }
    std::ofstream ofs(userConfigPath_);
    if (ofs.is_open()) {
        ofs << userLayer_;
        userDirty_ = false;
        FABRIC_LOG_DEBUG("Flushed user config to {}", userConfigPath_.string());
    } else {
        FABRIC_LOG_ERROR("Failed to write user config: {}", userConfigPath_.string());
    }
}

// -- Platform paths --

std::filesystem::path ConfigManager::userConfigDir() {
#if defined(_WIN32)
    const char* appdata = std::getenv("APPDATA");
    return appdata ? std::filesystem::path(appdata) / "Fabric" : std::filesystem::path(".");
#elif defined(__APPLE__)
    const char* home = std::getenv("HOME");
    return home ? std::filesystem::path(home) / "Library" / "Application Support" / "Fabric"
                : std::filesystem::path(".");
#else
    const char* xdg = std::getenv("XDG_CONFIG_HOME");
    if (xdg)
        return std::filesystem::path(xdg) / "fabric";
    const char* home = std::getenv("HOME");
    return home ? std::filesystem::path(home) / ".config" / "fabric" : std::filesystem::path(".");
#endif
}

std::filesystem::path ConfigManager::defaultUserConfigPath() {
    return userConfigDir() / "user.toml";
}

std::filesystem::path ConfigManager::findConfig(const std::filesystem::path& filename) {
    // 1. Current working directory
    if (std::filesystem::exists(filename)) {
        return filename;
    }

    // 2. Executable directory
    const char* base = SDL_GetBasePath();
    if (base != nullptr) {
        auto candidate = std::filesystem::path(base) / filename;
        if (std::filesystem::exists(candidate)) {
            return candidate;
        }
    }

    // 3. Source config directory (config/ relative to CWD)
    auto sourceConfig = std::filesystem::current_path() / "config" / filename;
    if (std::filesystem::exists(sourceConfig)) {
        return sourceConfig;
    }

    // Fall back to original filename (will fail with clear error in load* methods)
    return filename;
}

// -- Internal --

void ConfigManager::rebuildMerged() {
    merged_ = defaults_;
    deepMerge(merged_, engineLayer_);
    deepMerge(merged_, appLayer_);
    deepMerge(merged_, userLayer_);
    deepMerge(merged_, cliLayer_);
}

void ConfigManager::deepMerge(toml::table& target, const toml::table& source) {
    for (const auto& [key, val] : source) {
        if (val.is_table()) {
            auto* existing = target.get(key);
            if (existing && existing->is_table()) {
                deepMerge(*existing->as_table(), *val.as_table());
            } else {
                target.insert_or_assign(key, *val.as_table());
            }
        } else if (val.is_array()) {
            target.insert_or_assign(key, *val.as_array());
        } else if (val.is_string()) {
            target.insert_or_assign(key, val.as_string()->get());
        } else if (val.is_integer()) {
            target.insert_or_assign(key, val.as_integer()->get());
        } else if (val.is_floating_point()) {
            target.insert_or_assign(key, val.as_floating_point()->get());
        } else if (val.is_boolean()) {
            target.insert_or_assign(key, val.as_boolean()->get());
        }
    }
}

} // namespace fabric
