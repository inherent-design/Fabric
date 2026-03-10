#include "fabric/core/LogConfig.hh"
#include "fabric/platform/ConfigManager.hh"

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
    defaultLogger.consoleLevel = LogLevel::Info;
    defaultLogger.fileLevel = LogLevel::Debug;
    defaultLogger.consoleEnabled = true;
    defaultLogger.fileEnabled = true;

    cfg.loggers["fabric"] = defaultLogger;
    cfg.loggers["render"] = defaultLogger;
    cfg.loggers["physics"] = defaultLogger;
    cfg.loggers["audio"] = defaultLogger;

    // bgfx: console disabled by default (too verbose for TTY)
    LoggerConfig bgfxConfig;
    bgfxConfig.consoleLevel = LogLevel::Off;
    bgfxConfig.consoleEnabled = false; // No console output by default
    bgfxConfig.fileLevel = LogLevel::Debug;
    bgfxConfig.fileEnabled = true;
    bgfxConfig.fileName = "bgfx.log";
    cfg.loggers["bgfx"] = bgfxConfig;

    // Default: exclude bgfx and vulkan from console
    cfg.consoleExcludePatterns = {"bgfx.*", "vulkan.*"};

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
                loggerCfg.consoleLevel = lvl;
            }
        }

        // Global file level
        if (auto level = (*logging)["file_level"].value<std::string>()) {
            LogLevel lvl = parseLevelInternal(*level);
            for (auto& [name, loggerCfg] : cfg.loggers) {
                loggerCfg.fileLevel = lvl;
            }
        }

        // Global level (legacy support - applies to both console and file)
        if (auto level = (*logging)["level"].value<std::string>()) {
            LogLevel lvl = parseLevelInternal(*level);
            for (auto& [name, loggerCfg] : cfg.loggers) {
                loggerCfg.consoleLevel = lvl;
                loggerCfg.fileLevel = lvl;
            }
        }

        // Per-run folders toggle
        if (auto perRun = (*logging)["per_run_folders"].value<bool>()) {
            cfg.usePerRunFolders = *perRun;
        }

        // Logs directory
        if (auto logsDir = (*logging)["logsDir"].value<std::string>()) {
            cfg.logsDir = *logsDir;
        }

        // Console include patterns
        if (auto arr = (*logging)["console_include"].as_array()) {
            cfg.consoleIncludePatterns.clear();
            for (auto& elem : *arr) {
                if (auto pattern = elem.value<std::string>()) {
                    cfg.consoleIncludePatterns.push_back(*pattern);
                }
            }
        }

        // Console exclude patterns
        if (auto arr = (*logging)["console_exclude"].as_array()) {
            cfg.consoleExcludePatterns.clear();
            for (auto& elem : *arr) {
                if (auto pattern = elem.value<std::string>()) {
                    cfg.consoleExcludePatterns.push_back(*pattern);
                }
            }
        }

        // Per-logger overrides: [logging.loggers.<name>]
        if (auto loggers = (*logging)["loggers"].as_table()) {
            for (auto& [name, node] : *loggers) {
                std::string loggerName(name);
                if (auto loggerTbl = node.as_table()) {
                    auto& lc = cfg.loggers.try_emplace(loggerName, LoggerConfig{}).first->second;

                    if (auto level = (*loggerTbl)["console_level"].value<std::string>()) {
                        lc.consoleLevel = parseLevelInternal(*level);
                    }
                    if (auto level = (*loggerTbl)["file_level"].value<std::string>()) {
                        lc.fileLevel = parseLevelInternal(*level);
                    }
                    if (auto enabled = (*loggerTbl)["console_enabled"].value<bool>()) {
                        lc.consoleEnabled = *enabled;
                    }
                    if (auto enabled = (*loggerTbl)["file_enabled"].value<bool>()) {
                        lc.fileEnabled = *enabled;
                    }
                    if (auto fileName = (*loggerTbl)["file_name"].value<std::string>()) {
                        lc.fileName = *fileName;
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
            loggerCfg.consoleLevel = lvl;
            loggerCfg.fileLevel = lvl;
        }
    }

    // Console level override
    if (const char* level = std::getenv("FABRIC_LOG_CONSOLE")) {
        LogLevel lvl = parseLevelInternal(level);
        for (auto& [name, loggerCfg] : cfg.loggers) {
            loggerCfg.consoleLevel = lvl;
        }
    }

    // File level override
    if (const char* level = std::getenv("FABRIC_LOG_FILE")) {
        LogLevel lvl = parseLevelInternal(level);
        for (auto& [name, loggerCfg] : cfg.loggers) {
            loggerCfg.fileLevel = lvl;
        }
    }

    // Per-logger overrides
    if (const char* level = std::getenv("FABRIC_LOG_BGFX")) {
        if (cfg.loggers.find("bgfx") != cfg.loggers.end()) {
            LogLevel lvl = parseLevelInternal(level);
            cfg.loggers["bgfx"].consoleLevel = lvl;
            cfg.loggers["bgfx"].fileLevel = lvl;
            // Enable console if explicitly setting level
            if (lvl != LogLevel::Off) {
                cfg.loggers["bgfx"].consoleEnabled = true;
            }
        }
    }

    if (const char* level = std::getenv("FABRIC_LOG_RENDER")) {
        if (cfg.loggers.find("render") != cfg.loggers.end()) {
            LogLevel lvl = parseLevelInternal(level);
            cfg.loggers["render"].consoleLevel = lvl;
            cfg.loggers["render"].fileLevel = lvl;
        }
    }

    if (const char* level = std::getenv("FABRIC_LOG_PHYSICS")) {
        if (cfg.loggers.find("physics") != cfg.loggers.end()) {
            LogLevel lvl = parseLevelInternal(level);
            cfg.loggers["physics"].consoleLevel = lvl;
            cfg.loggers["physics"].fileLevel = lvl;
        }
    }

    if (const char* level = std::getenv("FABRIC_LOG_AUDIO")) {
        if (cfg.loggers.find("audio") != cfg.loggers.end()) {
            LogLevel lvl = parseLevelInternal(level);
            cfg.loggers["audio"].consoleLevel = lvl;
            cfg.loggers["audio"].fileLevel = lvl;
        }
    }

    // Console include patterns
    if (const char* include = std::getenv("FABRIC_LOG_CONSOLE_INCLUDE")) {
        cfg.consoleIncludePatterns = splitByComma(include);
    }

    // Console exclude patterns
    if (const char* exclude = std::getenv("FABRIC_LOG_CONSOLE_EXCLUDE")) {
        cfg.consoleExcludePatterns = splitByComma(exclude);
    }

    // Per-run folders toggle
    if (const char* perRun = std::getenv("FABRIC_LOG_PER_RUN")) {
        std::string val = perRun;
        for (char& c : val) {
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        cfg.usePerRunFolders = (val == "true" || val == "1" || val == "yes");
    }

    // Logs directory
    if (const char* logsDir = std::getenv("FABRIC_LOG_DIR")) {
        cfg.logsDir = logsDir;
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
                loggerCfg.consoleLevel = lvl;
                loggerCfg.fileLevel = lvl;
            }
        }
        // --log.console=LEVEL
        else if (arg.rfind("--log.console=", 0) == 0) {
            LogLevel lvl = parseLevelInternal(arg.substr(14));
            for (auto& [name, loggerCfg] : loggers) {
                loggerCfg.consoleLevel = lvl;
            }
        }
        // --log.file=LEVEL
        else if (arg.rfind("--log.file=", 0) == 0) {
            LogLevel lvl = parseLevelInternal(arg.substr(11));
            for (auto& [name, loggerCfg] : loggers) {
                loggerCfg.fileLevel = lvl;
            }
        }
        // --log.bgfx=LEVEL
        else if (arg.rfind("--log.bgfx=", 0) == 0) {
            if (loggers.find("bgfx") != loggers.end()) {
                LogLevel lvl = parseLevelInternal(arg.substr(11));
                loggers["bgfx"].consoleLevel = lvl;
                loggers["bgfx"].fileLevel = lvl;
                if (lvl != LogLevel::Off) {
                    loggers["bgfx"].consoleEnabled = true;
                }
            }
        }
        // --log.render=LEVEL
        else if (arg.rfind("--log.render=", 0) == 0) {
            if (loggers.find("render") != loggers.end()) {
                LogLevel lvl = parseLevelInternal(arg.substr(13));
                loggers["render"].consoleLevel = lvl;
                loggers["render"].fileLevel = lvl;
            }
        }
        // --log.physics=LEVEL
        else if (arg.rfind("--log.physics=", 0) == 0) {
            if (loggers.find("physics") != loggers.end()) {
                LogLevel lvl = parseLevelInternal(arg.substr(14));
                loggers["physics"].consoleLevel = lvl;
                loggers["physics"].fileLevel = lvl;
            }
        }
        // --log.audio=LEVEL
        else if (arg.rfind("--log.audio=", 0) == 0) {
            if (loggers.find("audio") != loggers.end()) {
                LogLevel lvl = parseLevelInternal(arg.substr(12));
                loggers["audio"].consoleLevel = lvl;
                loggers["audio"].fileLevel = lvl;
            }
        }
        // --log.include=PATTERN
        else if (arg.rfind("--log.include=", 0) == 0) {
            consoleIncludePatterns.push_back(arg.substr(14));
        }
        // --log.exclude=PATTERN
        else if (arg.rfind("--log.exclude=", 0) == 0) {
            consoleExcludePatterns.push_back(arg.substr(14));
        }
        // --log.per-run=BOOL
        else if (arg.rfind("--log.per-run=", 0) == 0) {
            std::string val = arg.substr(14);
            for (char& c : val) {
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            }
            usePerRunFolders = (val == "true" || val == "1" || val == "yes");
        }
        // --log.dir=PATH
        else if (arg.rfind("--log.dir=", 0) == 0) {
            logsDir = arg.substr(10);
        }
    }
}

void LogConfig::mergeFrom(const LogConfig& other) {
    // Override global settings from other (if non-default)
    if (!other.runId.empty()) {
        runId = other.runId;
    }
    if (!other.logsDir.empty() && other.logsDir != "logs") {
        logsDir = other.logsDir;
    }
    // Boolean: just copy
    usePerRunFolders = other.usePerRunFolders;

    // Merge loggers
    for (const auto& [name, otherCfg] : other.loggers) {
        loggers[name] = otherCfg;
    }

    // Merge patterns (append, don't replace)
    for (const auto& pattern : other.consoleIncludePatterns) {
        consoleIncludePatterns.push_back(pattern);
    }
    for (const auto& pattern : other.consoleExcludePatterns) {
        consoleExcludePatterns.push_back(pattern);
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
