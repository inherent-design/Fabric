#pragma once

// Fabric Engine Logging Configuration
// Unified configuration for logging subsystem with support for
// TOML config files, environment variables, and CLI flags.
//
// Priority: CLI > Env > TOML > Defaults

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include <toml++/toml.hpp>

namespace fabric {
class ConfigManager;
}

namespace fabric::log {

/// Log level for a specific destination (console/file)
/// Maps to Quill's internal levels with explicit ordering
enum class LogLevel : uint8_t {
    Off = 0,
    Critical = 1,
    Error = 2,
    Warning = 3,
    Info = 4,
    Debug = 5,
    Trace = 6
};

/// Configuration for a single logger (e.g., "fabric", "render", "bgfx")
struct LoggerConfig {
    LogLevel console_level = LogLevel::Info;
    LogLevel file_level = LogLevel::Debug;
    bool console_enabled = true;
    bool file_enabled = true;
    std::string file_name; // Empty = use logger name + ".log"
};

/// Complete logging configuration
struct LogConfig {
    // Global settings
    std::string run_id;              // Timestamped folder name (e.g., "2026-03-04_14-30-00")
    std::string logs_dir = "logs";   // Base directory for log files
    bool use_per_run_folders = true; // Create timestamped subdirectories

    // Per-logger configs (key = logger name: "fabric", "render", "physics", "audio", "bgfx")
    std::map<std::string, LoggerConfig> loggers;

    // Console filtering: only show logs from these patterns
    // Pattern syntax: "fabric.*" matches "fabric", "fabric.render", etc.
    std::vector<std::string> console_include_patterns;
    std::vector<std::string> console_exclude_patterns;

    /// Create default configuration with standard loggers
    static LogConfig fromDefaults();

    /// Load configuration from TOML file
    /// @param path Path to TOML config file (e.g., "config/fabric.toml")
    /// @return LogConfig with values from [logging] section
    static LogConfig fromTOML(const std::string& path);

    /// Load configuration from ConfigManager's merged table
    /// Uses the fully merged config (defaults < engine < app < user < CLI)
    /// @param config ConfigManager with loaded layers
    /// @return LogConfig with values from [logging] section
    static LogConfig fromConfigManager(const fabric::ConfigManager& config);

    /// Load configuration from a toml::table directly
    /// Useful when you already have a parsed TOML table
    /// @param tbl TOML table containing [logging] section
    /// @return LogConfig with values from [logging] section
    static LogConfig fromTOMLTable(const toml::table& tbl);

    /// Load configuration from environment variables
    /// Recognized vars:
    ///   FABRIC_LOG_LEVEL - Global level (debug, info, warn, etc.)
    ///   FABRIC_LOG_CONSOLE - Console level override
    ///   FABRIC_LOG_FILE - File level override
    ///   FABRIC_LOG_BGFX - bgfx logger level
    ///   FABRIC_LOG_RENDER - render logger level
    ///   FABRIC_LOG_CONSOLE_INCLUDE - Comma-separated include patterns
    ///   FABRIC_LOG_CONSOLE_EXCLUDE - Comma-separated exclude patterns
    ///   FABRIC_LOG_PER_RUN - "true"/"false" for per-run folders
    static LogConfig fromEnv();

    /// Parse CLI arguments and apply overrides
    /// Recognized flags:
    ///   --log.level=LEVEL         Global level
    ///   --log.console=LEVEL       Console level
    ///   --log.file=LEVEL          File level
    ///   --log.bgfx=LEVEL          bgfx level
    ///   --log.include=PATTERN     Console include pattern
    ///   --log.exclude=PATTERN     Console exclude pattern
    ///   --log.per-run=BOOL        Per-run folders toggle
    ///
    /// Modifies this config in-place with CLI overrides
    void applyCLIOverrides(int argc, char* argv[]);

    /// Merge another config into this one (other takes precedence)
    void mergeFrom(const LogConfig& other);

    /// Parse level string to enum (case-insensitive)
    /// Accepted: off, critical, error, warn, warning, info, debug, trace
    LogLevel parseLevel(const std::string& str) const;

    /// Convert LogLevel to string
    static std::string levelToString(LogLevel level);
};

} // namespace fabric::log
