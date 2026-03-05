#include "fabric/core/LogConfig.hh"
#include "fabric/core/ConfigManager.hh"

#include <cstdlib>
#include <filesystem>
#include <sstream>

// toml++ for config parsing
#include <toml++/toml.hpp>

namespace fabric::log {

namespace {

LogLevel parseLevelInternal(const std::string& str) {
    std::string lower;
    lower.reserve(str.size());
    for (char c : str) {
        lower.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }

    if (lower == "off" || lower == "none")
        return LogLevel::Off;
    if (lower == "critical" || lower == "fatal")
        return LogLevel::Critical;
    if (lower == "error" || lower == "err")
        return LogLevel::Error;
    if (lower == "warn" || lower == "warning")
        return LogLevel::Warning;
    if (lower == "info")
        return LogLevel::Info;
    if (lower == "debug" || lower == "dbg")
        return LogLevel::Debug;
    if (lower == "trace")
        return LogLevel::Trace;

    // Default to Info for unrecognized values
    return LogLevel::Info;
}

std::vector<std::string> splitByComma(const std::string& str) {
    std::vector<std::string> result;
    std::istringstream iss(str);
    std::string token;
    while (std::getline(iss, token, ',')) {
        // Trim whitespace
        size_t start = token.find_first_not_of(" \t");
        size_t end = token.find_last_not_of(" \t");
        if (start != std::string::npos && end != std::string::npos) {
            result.push_back(token.substr(start, end - start + 1));
        }
    }
    return result;
}

} // namespace

LogConfig LogConfig::fromDefaults() {
    LogConfig cfg;

    // Standard loggers with default settings
    LoggerConfig defaultLogger;
    defaultLogger.console_level = LogLevel::Info;
    defaultLogger.file_level = LogLevel::Debug;
    defaultLogger.console_enabled = true;
    defaultLogger.file_enabled = true;

    cfg.loggers["fabric"] = defaultLogger;
    cfg.loggers["render"] = defaultLogger;
    cfg.loggers["physics"] = defaultLogger;
    cfg.loggers["audio"] = defaultLogger;

    // bgfx: console disabled by default (too verbose for TTY)
    LoggerConfig bgfxConfig;
    bgfxConfig.console_level = LogLevel::Off;
    bgfxConfig.console_enabled = false; // No console output by default
    bgfxConfig.file_level = LogLevel::Debug;
    bgfxConfig.file_enabled = true;
    bgfxConfig.file_name = "bgfx.log";
    cfg.loggers["bgfx"] = bgfxConfig;

    // Default: exclude bgfx and vulkan from console
    cfg.console_exclude_patterns = {"bgfx.*", "vulkan.*"};

    return cfg;
}

LogConfig LogConfig::fromTOML(const std::string& path) {
    if (!std::filesystem::exists(path)) {
        return fromDefaults();
    }

    try {
        auto tbl = toml::parse_file(path);
        return fromTOMLTable(tbl);
    } catch (const toml::parse_error&) {
        // If TOML parsing fails, return defaults
        return fromDefaults();
    }
}

LogConfig LogConfig::fromConfigManager(const fabric::ConfigManager& config) {
    return fromTOMLTable(config.merged());
}

LogConfig LogConfig::fromTOMLTable(const toml::table& tbl) {
    LogConfig cfg = fromDefaults();

    // Look for [logging] section
    if (auto logging = tbl["logging"].as_table()) {
        // Global console level
        if (auto level = (*logging)["console_level"].value<std::string>()) {
            LogLevel lvl = parseLevelInternal(*level);
            for (auto& [name, loggerCfg] : cfg.loggers) {
                loggerCfg.console_level = lvl;
            }
        }

        // Global file level
        if (auto level = (*logging)["file_level"].value<std::string>()) {
            LogLevel lvl = parseLevelInternal(*level);
            for (auto& [name, loggerCfg] : cfg.loggers) {
                loggerCfg.file_level = lvl;
            }
        }

        // Global level (legacy support - applies to both console and file)
        if (auto level = (*logging)["level"].value<std::string>()) {
            LogLevel lvl = parseLevelInternal(*level);
            for (auto& [name, loggerCfg] : cfg.loggers) {
                loggerCfg.console_level = lvl;
                loggerCfg.file_level = lvl;
            }
        }

        // Per-run folders toggle
        if (auto perRun = (*logging)["per_run_folders"].value<bool>()) {
            cfg.use_per_run_folders = *perRun;
        }

        // Logs directory
        if (auto logsDir = (*logging)["logs_dir"].value<std::string>()) {
            cfg.logs_dir = *logsDir;
        }

        // Console include patterns
        if (auto arr = (*logging)["console_include"].as_array()) {
            cfg.console_include_patterns.clear();
            for (auto& elem : *arr) {
                if (auto pattern = elem.value<std::string>()) {
                    cfg.console_include_patterns.push_back(*pattern);
                }
            }
        }

        // Console exclude patterns
        if (auto arr = (*logging)["console_exclude"].as_array()) {
            cfg.console_exclude_patterns.clear();
            for (auto& elem : *arr) {
                if (auto pattern = elem.value<std::string>()) {
                    cfg.console_exclude_patterns.push_back(*pattern);
                }
            }
        }

        // Per-logger overrides: [logging.loggers.<name>]
        if (auto loggers = (*logging)["loggers"].as_table()) {
            for (auto& [name, node] : *loggers) {
                std::string loggerName(name);
                if (auto loggerTbl = node.as_table()) {
                    // Create or get logger config
                    if (cfg.loggers.find(loggerName) == cfg.loggers.end()) {
                        cfg.loggers[loggerName] = LoggerConfig{};
                    }

                    auto& lc = cfg.loggers[loggerName];

                    if (auto level = (*loggerTbl)["console_level"].value<std::string>()) {
                        lc.console_level = parseLevelInternal(*level);
                    }
                    if (auto level = (*loggerTbl)["file_level"].value<std::string>()) {
                        lc.file_level = parseLevelInternal(*level);
                    }
                    if (auto enabled = (*loggerTbl)["console_enabled"].value<bool>()) {
                        lc.console_enabled = *enabled;
                    }
                    if (auto enabled = (*loggerTbl)["file_enabled"].value<bool>()) {
                        lc.file_enabled = *enabled;
                    }
                    if (auto fileName = (*loggerTbl)["file_name"].value<std::string>()) {
                        lc.file_name = *fileName;
                    }
                }
            }
        }
    }

    return cfg;
}

LogConfig LogConfig::fromEnv() {
    LogConfig cfg = fromDefaults();

    // Global log level
    if (const char* level = std::getenv("FABRIC_LOG_LEVEL")) {
        LogLevel lvl = parseLevelInternal(level);
        for (auto& [name, loggerCfg] : cfg.loggers) {
            loggerCfg.console_level = lvl;
            loggerCfg.file_level = lvl;
        }
    }

    // Console level override
    if (const char* level = std::getenv("FABRIC_LOG_CONSOLE")) {
        LogLevel lvl = parseLevelInternal(level);
        for (auto& [name, loggerCfg] : cfg.loggers) {
            loggerCfg.console_level = lvl;
        }
    }

    // File level override
    if (const char* level = std::getenv("FABRIC_LOG_FILE")) {
        LogLevel lvl = parseLevelInternal(level);
        for (auto& [name, loggerCfg] : cfg.loggers) {
            loggerCfg.file_level = lvl;
        }
    }

    // Per-logger overrides
    if (const char* level = std::getenv("FABRIC_LOG_BGFX")) {
        if (cfg.loggers.find("bgfx") != cfg.loggers.end()) {
            LogLevel lvl = parseLevelInternal(level);
            cfg.loggers["bgfx"].console_level = lvl;
            cfg.loggers["bgfx"].file_level = lvl;
            // Enable console if explicitly setting level
            if (lvl != LogLevel::Off) {
                cfg.loggers["bgfx"].console_enabled = true;
            }
        }
    }

    if (const char* level = std::getenv("FABRIC_LOG_RENDER")) {
        if (cfg.loggers.find("render") != cfg.loggers.end()) {
            LogLevel lvl = parseLevelInternal(level);
            cfg.loggers["render"].console_level = lvl;
            cfg.loggers["render"].file_level = lvl;
        }
    }

    if (const char* level = std::getenv("FABRIC_LOG_PHYSICS")) {
        if (cfg.loggers.find("physics") != cfg.loggers.end()) {
            LogLevel lvl = parseLevelInternal(level);
            cfg.loggers["physics"].console_level = lvl;
            cfg.loggers["physics"].file_level = lvl;
        }
    }

    if (const char* level = std::getenv("FABRIC_LOG_AUDIO")) {
        if (cfg.loggers.find("audio") != cfg.loggers.end()) {
            LogLevel lvl = parseLevelInternal(level);
            cfg.loggers["audio"].console_level = lvl;
            cfg.loggers["audio"].file_level = lvl;
        }
    }

    // Console include patterns
    if (const char* include = std::getenv("FABRIC_LOG_CONSOLE_INCLUDE")) {
        cfg.console_include_patterns = splitByComma(include);
    }

    // Console exclude patterns
    if (const char* exclude = std::getenv("FABRIC_LOG_CONSOLE_EXCLUDE")) {
        cfg.console_exclude_patterns = splitByComma(exclude);
    }

    // Per-run folders toggle
    if (const char* perRun = std::getenv("FABRIC_LOG_PER_RUN")) {
        std::string val = perRun;
        for (char& c : val) {
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        cfg.use_per_run_folders = (val == "true" || val == "1" || val == "yes");
    }

    // Logs directory
    if (const char* logsDir = std::getenv("FABRIC_LOG_DIR")) {
        cfg.logs_dir = logsDir;
    }

    return cfg;
}

void LogConfig::applyCLIOverrides(int argc, char* argv[]) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        // --log.level=LEVEL
        if (arg.rfind("--log.level=", 0) == 0) {
            LogLevel lvl = parseLevelInternal(arg.substr(12));
            for (auto& [name, loggerCfg] : loggers) {
                loggerCfg.console_level = lvl;
                loggerCfg.file_level = lvl;
            }
        }
        // --log.console=LEVEL
        else if (arg.rfind("--log.console=", 0) == 0) {
            LogLevel lvl = parseLevelInternal(arg.substr(14));
            for (auto& [name, loggerCfg] : loggers) {
                loggerCfg.console_level = lvl;
            }
        }
        // --log.file=LEVEL
        else if (arg.rfind("--log.file=", 0) == 0) {
            LogLevel lvl = parseLevelInternal(arg.substr(11));
            for (auto& [name, loggerCfg] : loggers) {
                loggerCfg.file_level = lvl;
            }
        }
        // --log.bgfx=LEVEL
        else if (arg.rfind("--log.bgfx=", 0) == 0) {
            if (loggers.find("bgfx") != loggers.end()) {
                LogLevel lvl = parseLevelInternal(arg.substr(11));
                loggers["bgfx"].console_level = lvl;
                loggers["bgfx"].file_level = lvl;
                if (lvl != LogLevel::Off) {
                    loggers["bgfx"].console_enabled = true;
                }
            }
        }
        // --log.render=LEVEL
        else if (arg.rfind("--log.render=", 0) == 0) {
            if (loggers.find("render") != loggers.end()) {
                LogLevel lvl = parseLevelInternal(arg.substr(13));
                loggers["render"].console_level = lvl;
                loggers["render"].file_level = lvl;
            }
        }
        // --log.physics=LEVEL
        else if (arg.rfind("--log.physics=", 0) == 0) {
            if (loggers.find("physics") != loggers.end()) {
                LogLevel lvl = parseLevelInternal(arg.substr(14));
                loggers["physics"].console_level = lvl;
                loggers["physics"].file_level = lvl;
            }
        }
        // --log.audio=LEVEL
        else if (arg.rfind("--log.audio=", 0) == 0) {
            if (loggers.find("audio") != loggers.end()) {
                LogLevel lvl = parseLevelInternal(arg.substr(12));
                loggers["audio"].console_level = lvl;
                loggers["audio"].file_level = lvl;
            }
        }
        // --log.include=PATTERN
        else if (arg.rfind("--log.include=", 0) == 0) {
            console_include_patterns.push_back(arg.substr(14));
        }
        // --log.exclude=PATTERN
        else if (arg.rfind("--log.exclude=", 0) == 0) {
            console_exclude_patterns.push_back(arg.substr(14));
        }
        // --log.per-run=BOOL
        else if (arg.rfind("--log.per-run=", 0) == 0) {
            std::string val = arg.substr(14);
            for (char& c : val) {
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            }
            use_per_run_folders = (val == "true" || val == "1" || val == "yes");
        }
        // --log.dir=PATH
        else if (arg.rfind("--log.dir=", 0) == 0) {
            logs_dir = arg.substr(10);
        }
    }
}

void LogConfig::mergeFrom(const LogConfig& other) {
    // Override global settings from other (if non-default)
    if (!other.run_id.empty()) {
        run_id = other.run_id;
    }
    if (!other.logs_dir.empty() && other.logs_dir != "logs") {
        logs_dir = other.logs_dir;
    }
    // Boolean: just copy
    use_per_run_folders = other.use_per_run_folders;

    // Merge loggers
    for (const auto& [name, otherCfg] : other.loggers) {
        loggers[name] = otherCfg;
    }

    // Merge patterns (append, don't replace)
    for (const auto& pattern : other.console_include_patterns) {
        console_include_patterns.push_back(pattern);
    }
    for (const auto& pattern : other.console_exclude_patterns) {
        console_exclude_patterns.push_back(pattern);
    }
}

LogLevel LogConfig::parseLevel(const std::string& str) const {
    return parseLevelInternal(str);
}

std::string LogConfig::levelToString(LogLevel level) {
    switch (level) {
        case LogLevel::Off:
            return "off";
        case LogLevel::Critical:
            return "critical";
        case LogLevel::Error:
            return "error";
        case LogLevel::Warning:
            return "warning";
        case LogLevel::Info:
            return "info";
        case LogLevel::Debug:
            return "debug";
        case LogLevel::Trace:
            return "trace";
        default:
            return "info";
    }
}

} // namespace fabric::log
