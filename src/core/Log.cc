#include "fabric/core/Log.hh"
#include "fabric/core/FilteredConsoleSink.hh"

#include <quill/Backend.h>
#include <quill/Frontend.h>
#include <quill/Logger.h>
#include <quill/sinks/ConsoleSink.h>
#include <quill/sinks/FileSink.h>

#include <chrono>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <string>

namespace fabric::log {

namespace {
// Root logger (all channels aggregated)
quill::Logger* g_logger = nullptr;

// Per-subsystem named loggers (only those with active callers)
quill::Logger* g_logger_render = nullptr;
quill::Logger* g_logger_physics = nullptr;
quill::Logger* g_logger_audio = nullptr;
quill::Logger* g_logger_bgfx = nullptr;

const std::string kLogsDir = "logs";

std::shared_ptr<quill::Sink> makeFileSink(const std::string& filename) {
    return quill::Frontend::create_or_get_sink<quill::FileSink>(filename, []() {
        quill::FileSinkConfig cfg;
        cfg.set_open_mode('w');
        cfg.set_filename_append_option(quill::FilenameAppendOption::StartDateTime);
        return cfg;
    }());
}

void createLogDirectory() {
    std::filesystem::create_directories(kLogsDir);
}

void setAllLoggersInfoLevel() {
    for (auto* lg : {g_logger_render, g_logger_physics, g_logger_audio, g_logger_bgfx}) {
        if (lg)
            lg->set_log_level(quill::LogLevel::Info);
    }
}

/// Create a timestamped run folder and update "latest" symlink
std::string createRunFolder(const std::string& baseDir) {
    using namespace std::chrono;
    auto now = system_clock::now();
    auto time = system_clock::to_time_t(now);

    std::ostringstream ss;
    ss << std::put_time(std::localtime(&time), "%Y-%m-%d_%H-%M-%S");
    std::string runId = ss.str();

    std::filesystem::path runPath = std::filesystem::path(baseDir) / runId;
    std::filesystem::create_directories(runPath);

    // Create/update "latest" symlink to most recent run
    std::filesystem::path latestPath = std::filesystem::path(baseDir) / "latest";

    // Remove existing symlink if present (may be file or directory)
    std::error_code ec;
    std::filesystem::remove(latestPath, ec);

    // Create new symlink (use relative path for portability)
    std::filesystem::create_directory_symlink(runPath.filename(), latestPath, ec);

    // If symlink failed (e.g., Windows without admin), continue without it
    // The logs will still be in the timestamped folder

    return runPath.string();
}

/// Convert LogLevel to Quill's LogLevel
quill::LogLevel toQuillLevel(LogLevel level) {
    switch (level) {
        case LogLevel::Off:
            return quill::LogLevel::None;
        case LogLevel::Critical:
            return quill::LogLevel::Critical;
        case LogLevel::Error:
            return quill::LogLevel::Error;
        case LogLevel::Warning:
            return quill::LogLevel::Warning;
        case LogLevel::Info:
            return quill::LogLevel::Info;
        case LogLevel::Debug:
            return quill::LogLevel::Debug;
        case LogLevel::Trace:
            return quill::LogLevel::TraceL1;
        default:
            return quill::LogLevel::Info;
    }
}

/// Pattern for log messages
quill::PatternFormatterOptions makePatternOptions() {
    quill::PatternFormatterOptions pattern;
    pattern.format_pattern = "%(time) [%(thread_id)] %(short_source_location:<28) "
                             "%(log_level:<9) %(message)";
    pattern.timestamp_pattern = "%H:%M:%S.%Qms";
    return pattern;
}

} // namespace

void init(const LogConfig& config) {
    quill::BackendOptions backend_opts;
    backend_opts.thread_name = "FabricLog";
    backend_opts.wait_for_queues_to_empty_before_exit = true;

    quill::Backend::start(backend_opts);

    // Determine log path (with per-run folder if enabled)
    std::filesystem::path logPath = config.logsDir;
    if (config.usePerRunFolders) {
        logPath = createRunFolder(config.logsDir);
    } else {
        std::filesystem::create_directories(logPath);
    }

    // Create filtered console sink with include/exclude patterns
    auto filtered_console = std::make_shared<FilteredConsoleSink>();
    filtered_console->setIncludePatterns(config.consoleIncludePatterns);
    filtered_console->setExcludePatterns(config.consoleExcludePatterns);

    auto console_sink = filtered_console;
    auto pattern = makePatternOptions();

    // Create loggers from config
    for (const auto& [name, cfg] : config.loggers) {
        std::vector<std::shared_ptr<quill::Sink>> sinks;

        // Add console sink if enabled
        if (cfg.consoleEnabled) {
            sinks.push_back(console_sink);
        }

        // Add file sink if enabled
        if (cfg.fileEnabled) {
            std::string fileName = cfg.fileName.empty() ? name + ".log" : cfg.fileName;
            auto filePath = (logPath / fileName).string();
            sinks.push_back(makeFileSink(filePath));
        }

        // Create logger (skip if no sinks)
        if (sinks.empty()) {
            continue;
        }

        quill::Logger* logger = quill::Frontend::create_or_get_logger(name, sinks, pattern);

        // Set log level (use the higher of console/file for the logger's level,
        // since Quill uses a single level per logger)
        LogLevel maxLevel = static_cast<uint8_t>(cfg.consoleLevel) > static_cast<uint8_t>(cfg.fileLevel)
                                ? cfg.consoleLevel
                                : cfg.fileLevel;
        quill::LogLevel quillLevel = toQuillLevel(maxLevel);
        logger->set_log_level(quillLevel);

        // Store in global pointers for known loggers
        if (name == "fabric") {
            g_logger = logger;
        } else if (name == "render") {
            g_logger_render = logger;
        } else if (name == "physics") {
            g_logger_physics = logger;
        } else if (name == "audio") {
            g_logger_audio = logger;
        } else if (name == "bgfx") {
            g_logger_bgfx = logger;
        }
    }

    // Ensure fabric logger exists even if not in config
    if (!g_logger) {
        g_logger = quill::Frontend::create_or_get_logger("fabric", console_sink, pattern);
        g_logger->set_log_level(quill::LogLevel::Info);
    }
}

void init() {
    // Use default config with per-run folders
    init(LogConfig::fromDefaults());
}

void init(const char* log_file_path) {
    quill::BackendOptions backend_opts;
    backend_opts.thread_name = "FabricLog";
    backend_opts.wait_for_queues_to_empty_before_exit = true;

    quill::Backend::start(backend_opts);

    createLogDirectory();

    auto console_sink = quill::Frontend::create_or_get_sink<quill::ConsoleSink>("console");

    // Legacy file sink from caller-provided path
    auto file_sink = makeFileSink(log_file_path);

    quill::PatternFormatterOptions pattern;
    pattern.format_pattern = "%(time) [%(thread_id)] %(short_source_location:<28) "
                             "%(log_level:<9) %(message)";
    pattern.timestamp_pattern = "%H:%M:%S.%Qms";

    // Per-subsystem file sinks
    auto fabric_file = makeFileSink(kLogsDir + "/fabric.log");
    auto render_file = makeFileSink(kLogsDir + "/render.log");

    // Root logger: console + caller file + fabric.log
    g_logger = quill::Frontend::create_or_get_logger("fabric", {console_sink, file_sink, fabric_file}, pattern);
    g_logger->set_log_level(quill::LogLevel::Info);

    // Render logger: console + caller file + render.log
    g_logger_render = quill::Frontend::create_or_get_logger("render", {console_sink, file_sink, render_file}, pattern);

    // Remaining subsystem loggers: console + caller file
    g_logger_physics = quill::Frontend::create_or_get_logger("physics", {console_sink, file_sink}, pattern);
    g_logger_audio = quill::Frontend::create_or_get_logger("audio", {console_sink, file_sink}, pattern);
    g_logger_bgfx = quill::Frontend::create_or_get_logger("bgfx", {console_sink, file_sink}, pattern);

    setAllLoggersInfoLevel();
}

void shutdown() {
    if (g_logger) {
        g_logger->flush_log();
    }
    for (auto* lg : {g_logger_render, g_logger_physics, g_logger_audio, g_logger_bgfx}) {
        if (lg)
            lg->flush_log();
    }
    quill::Backend::stop();
}

quill::Logger* logger() {
    if (!g_logger) {
        // Lazy-init a console-only logger so logging before init() doesn't
        // dereference nullptr. Uses a distinct name so init() can still
        // create the real "fabric" logger with file sinks later.
        quill::BackendOptions opts;
        opts.thread_name = "FabricLog";
        opts.wait_for_queues_to_empty_before_exit = true;
        quill::Backend::start(opts);

        auto console = quill::Frontend::create_or_get_sink<quill::ConsoleSink>("console");
        g_logger = quill::Frontend::create_or_get_logger("fabric_preinit", console);
        g_logger->set_log_level(quill::LogLevel::Info);
    }
    return g_logger;
}

quill::Logger* renderLogger() {
    return g_logger_render;
}

quill::Logger* physicsLogger() {
    return g_logger_physics;
}

quill::Logger* audioLogger() {
    return g_logger_audio;
}

quill::Logger* bgfxLogger() {
    return g_logger_bgfx;
}

void setLevel(quill::LogLevel level) {
    if (g_logger) {
        g_logger->set_log_level(level);
    }
}

void setRenderLevel(quill::LogLevel level) {
    if (g_logger_render)
        g_logger_render->set_log_level(level);
}

void setPhysicsLevel(quill::LogLevel level) {
    if (g_logger_physics)
        g_logger_physics->set_log_level(level);
}

void setAudioLevel(quill::LogLevel level) {
    if (g_logger_audio)
        g_logger_audio->set_log_level(level);
}

void setBgfxLevel(quill::LogLevel level) {
    if (g_logger_bgfx)
        g_logger_bgfx->set_log_level(level);
}

} // namespace fabric::log
