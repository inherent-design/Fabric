#include "fabric/core/ConfigManager.hh"

#include <cstdlib>
#include <fstream>
#include <string>

#include "fabric/core/DefaultConfig.hh"
#include "fabric/core/Log.hh"

namespace fabric {

// -- Construction --

ConfigManager::ConfigManager() : userConfigPath_(defaultUserConfigPath()) {
    // Build Layer 0 from compiled defaults
    toml::table window;
    window.insert_or_assign("title", DefaultConfig::kWindowTitle);
    window.insert_or_assign("width", static_cast<int64_t>(DefaultConfig::kWindowWidth));
    window.insert_or_assign("height", static_cast<int64_t>(DefaultConfig::kWindowHeight));
    window.insert_or_assign("min_width", static_cast<int64_t>(DefaultConfig::kMinWindowWidth));
    window.insert_or_assign("min_height", static_cast<int64_t>(DefaultConfig::kMinWindowHeight));
    window.insert_or_assign("display", static_cast<int64_t>(DefaultConfig::kDisplay));
    window.insert_or_assign("fullscreen", DefaultConfig::kFullscreen);
    window.insert_or_assign("borderless", DefaultConfig::kBorderless);
    window.insert_or_assign("resizable", DefaultConfig::kResizable);
    window.insert_or_assign("hidpi", DefaultConfig::kHiDPI);
    window.insert_or_assign("maximized", DefaultConfig::kMaximized);

    toml::table renderer;
    renderer.insert_or_assign("backend", DefaultConfig::kRendererBackend);
    renderer.insert_or_assign("debug", DefaultConfig::kRendererDebug);
    renderer.insert_or_assign("vsync", DefaultConfig::kVsync);

    toml::table logging;
    logging.insert_or_assign("level", DefaultConfig::kLogLevel);
    logging.insert_or_assign("file_sink", DefaultConfig::kFileSink);
    logging.insert_or_assign("console_sink", DefaultConfig::kConsoleSink);

    toml::table profiling;
    profiling.insert_or_assign("enabled", DefaultConfig::kProfilingEnabled);
    profiling.insert_or_assign("connect_address", DefaultConfig::kProfilingConnectAddress);

    toml::table platform;
    platform.insert_or_assign("mimalloc_override", DefaultConfig::kMimallocOverride);

    defaults_.insert_or_assign("window", std::move(window));
    defaults_.insert_or_assign("renderer", std::move(renderer));
    defaults_.insert_or_assign("logging", std::move(logging));
    defaults_.insert_or_assign("profiling", std::move(profiling));
    defaults_.insert_or_assign("platform", std::move(platform));

    rebuildMerged();
}

// -- Layer loading --

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

        if (val == "true") {
            insertValueAtPath(cliLayer_, key, true);
        } else if (val == "false") {
            insertValueAtPath(cliLayer_, key, false);
        } else {
            // Try integer (no decimal point)
            if (val.find('.') == std::string::npos) {
                try {
                    size_t pos = 0;
                    int64_t intVal = std::stoll(val, &pos);
                    if (pos == val.size()) {
                        insertValueAtPath(cliLayer_, key, intVal);
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
                    insertValueAtPath(cliLayer_, key, floatVal);
                    continue;
                }
            } catch (...) {
                // Fall through to string
            }
            // String fallback
            insertValueAtPath(cliLayer_, key, val);
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
    if (now - lastDirtyTime_ < kDebounceMs)
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
