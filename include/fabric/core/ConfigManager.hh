#pragma once

#include <chrono>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>

#include <toml++/toml.hpp>

namespace fabric {

/// 5-layer configuration manager with typed access.
/// Precedence: compiled defaults < fabric.toml < app.toml < user.toml < CLI flags.
/// Thread-safe for reads; writes are guarded by mutex.
class ConfigManager {
  public:
    ConfigManager();

    /// Load engine config (fabric.toml) from the given path.
    /// Merges on top of compiled defaults.
    void loadEngineConfig(const std::filesystem::path& path);

    /// Load application config (recurse.toml) from the given path.
    /// Merges on top of engine config.
    void loadAppConfig(const std::filesystem::path& path);

    /// Load user preferences from the given path.
    /// Merges on top of application config.
    void loadUserConfig(const std::filesystem::path& path);

    /// Apply CLI flag overrides. Parses --section.key=value arguments.
    void applyCLIOverrides(int argc, char* argv[]);

    /// Typed read. Returns value at dotted key path, or std::nullopt if missing.
    template <typename T> std::optional<T> get(std::string_view key) const;

    /// Typed read with default. Returns default if key is missing or wrong type.
    template <typename T> T get(std::string_view key, T defaultValue) const;

    /// Check if a key exists in the merged config.
    bool has(std::string_view key) const;

    /// Set a value in the user layer (persisted on next flush).
    /// Only the user layer is writable at runtime.
    template <typename T> void set(std::string_view key, T value);

    /// Flush pending user.toml writes if 500ms debounce has elapsed.
    /// Called automatically by the engine each frame.
    void flushIfDirty();

    /// Force immediate write of user.toml.
    void flushNow();

    /// Set the user config file path (overrides platform default).
    void setUserConfigPath(const std::filesystem::path& path);

    /// Return the resolved user config directory for the current platform.
    static std::filesystem::path userConfigDir();

    /// Return the full path to user.toml using the platform default.
    static std::filesystem::path defaultUserConfigPath();

    /// Search for a config file in multiple locations.
    /// Search order:
    ///   1. Current working directory
    ///   2. Executable directory
    ///   3. Source config directory (config/ relative to CWD)
    /// Returns the first found path, or the original filename if not found.
    static std::filesystem::path findConfig(const std::filesystem::path& filename);

    /// Access the merged table (read-only, for enumeration / debugging).
    const toml::table& merged() const;

  private:
    void rebuildMerged();
    static void deepMerge(toml::table& target, const toml::table& source);

    template <typename T> static void insertValueAtPath(toml::table& root, std::string_view path, T value);

    toml::table defaults_;
    toml::table engineLayer_;
    toml::table appLayer_;
    toml::table userLayer_;
    toml::table cliLayer_;
    toml::table merged_;

    std::filesystem::path userConfigPath_;
    bool userDirty_ = false;
    std::chrono::steady_clock::time_point lastDirtyTime_;

    mutable std::mutex mutex_;

    static constexpr auto kDebounceMs = std::chrono::milliseconds(500);
};

// -- Template implementations --

template <typename T> std::optional<T> ConfigManager::get(std::string_view key) const {
    std::lock_guard lock(mutex_);
    auto node = merged_.at_path(key);
    if (!node)
        return std::nullopt;

    if constexpr (std::is_same_v<T, int>) {
        auto val = node.value<int64_t>();
        if (val)
            return static_cast<int>(*val);
        return std::nullopt;
    } else if constexpr (std::is_same_v<T, float>) {
        auto val = node.value<double>();
        if (val)
            return static_cast<float>(*val);
        return std::nullopt;
    } else {
        return node.value<T>();
    }
}

template <typename T> T ConfigManager::get(std::string_view key, T defaultValue) const {
    auto val = get<T>(key);
    return val.value_or(std::move(defaultValue));
}

template <typename T> void ConfigManager::set(std::string_view key, T value) {
    std::lock_guard lock(mutex_);
    insertValueAtPath(userLayer_, key, std::move(value));
    userDirty_ = true;
    lastDirtyTime_ = std::chrono::steady_clock::now();
    rebuildMerged();
}

template <typename T> void ConfigManager::insertValueAtPath(toml::table& root, std::string_view path, T value) {
    auto dot = path.find('.');
    if (dot == std::string_view::npos) {
        if constexpr (std::is_same_v<T, int>) {
            root.insert_or_assign(path, static_cast<int64_t>(value));
        } else if constexpr (std::is_same_v<T, float>) {
            root.insert_or_assign(path, static_cast<double>(value));
        } else {
            root.insert_or_assign(path, std::move(value));
        }
        return;
    }

    auto segment = path.substr(0, dot);
    auto rest = path.substr(dot + 1);
    auto* node = root.get(segment);
    if (!node || !node->is_table()) {
        root.insert_or_assign(segment, toml::table{});
        node = root.get(segment);
    }
    insertValueAtPath(*node->as_table(), rest, std::move(value));
}

} // namespace fabric
