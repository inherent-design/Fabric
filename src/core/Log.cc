#include "fabric/core/Log.hh"

#include <quill/Backend.h>
#include <quill/Frontend.h>
#include <quill/Logger.h>
#include <quill/sinks/ConsoleSink.h>
#include <quill/sinks/FileSink.h>

namespace fabric::log {

namespace {
// Root logger (all channels aggregated)
quill::Logger* g_logger = nullptr;

// Per-subsystem named loggers
quill::Logger* g_logger_core = nullptr;
quill::Logger* g_logger_render = nullptr;
quill::Logger* g_logger_terrain = nullptr;
quill::Logger* g_logger_physics = nullptr;
quill::Logger* g_logger_audio = nullptr;
quill::Logger* g_logger_input = nullptr;
quill::Logger* g_logger_ui = nullptr;
quill::Logger* g_logger_ecs = nullptr;
quill::Logger* g_logger_bgfx = nullptr;
} // namespace

void init() {
    quill::BackendOptions backend_opts;
    backend_opts.thread_name = "FabricLog";
    backend_opts.wait_for_queues_to_empty_before_exit = true;

    quill::Backend::start(backend_opts);

    auto console_sink = quill::Frontend::create_or_get_sink<quill::ConsoleSink>("console");

    quill::PatternFormatterOptions pattern;
    pattern.format_pattern = "%(time) [%(thread_id)] %(short_source_location:<28) "
                             "%(log_level:<9) %(message)";
    pattern.timestamp_pattern = "%H:%M:%S.%Qms";

    g_logger = quill::Frontend::create_or_get_logger("fabric", std::move(console_sink), pattern);
    g_logger->set_log_level(quill::LogLevel::Info);

    // Create per-subsystem loggers from the same console sink
    auto console_sink_ref = quill::Frontend::get_sink("console");
    g_logger_core = quill::Frontend::create_or_get_logger("core", console_sink_ref, pattern);
    g_logger_render = quill::Frontend::create_or_get_logger("render", console_sink_ref, pattern);
    g_logger_terrain = quill::Frontend::create_or_get_logger("terrain", console_sink_ref, pattern);
    g_logger_physics = quill::Frontend::create_or_get_logger("physics", console_sink_ref, pattern);
    g_logger_audio = quill::Frontend::create_or_get_logger("audio", console_sink_ref, pattern);
    g_logger_input = quill::Frontend::create_or_get_logger("input", console_sink_ref, pattern);
    g_logger_ui = quill::Frontend::create_or_get_logger("ui", console_sink_ref, pattern);
    g_logger_ecs = quill::Frontend::create_or_get_logger("ecs", console_sink_ref, pattern);
    g_logger_bgfx = quill::Frontend::create_or_get_logger("bgfx", console_sink_ref, pattern);

    // All loggers start at Info level
    for (auto logger : {g_logger_core, g_logger_render, g_logger_terrain, g_logger_physics, g_logger_audio,
                        g_logger_input, g_logger_ui, g_logger_ecs, g_logger_bgfx}) {
        if (logger)
            logger->set_log_level(quill::LogLevel::Info);
    }
}

void init(const char* log_file_path) {
    quill::BackendOptions backend_opts;
    backend_opts.thread_name = "FabricLog";
    backend_opts.wait_for_queues_to_empty_before_exit = true;

    quill::Backend::start(backend_opts);

    auto console_sink = quill::Frontend::create_or_get_sink<quill::ConsoleSink>("console");

    auto file_sink = quill::Frontend::create_or_get_sink<quill::FileSink>(log_file_path, []() {
        quill::FileSinkConfig cfg;
        cfg.set_open_mode('w');
        cfg.set_filename_append_option(quill::FilenameAppendOption::StartDateTime);
        return cfg;
    }());

    quill::PatternFormatterOptions pattern;
    pattern.format_pattern = "%(time) [%(thread_id)] %(short_source_location:<28) "
                             "%(log_level:<9) %(message)";
    pattern.timestamp_pattern = "%H:%M:%S.%Qms";

    g_logger = quill::Frontend::create_or_get_logger("fabric", {console_sink, file_sink}, pattern);
    g_logger->set_log_level(quill::LogLevel::Info);

    // Create per-subsystem loggers with both sinks
    g_logger_core = quill::Frontend::create_or_get_logger("core", {console_sink, file_sink}, pattern);
    g_logger_render = quill::Frontend::create_or_get_logger("render", {console_sink, file_sink}, pattern);
    g_logger_terrain = quill::Frontend::create_or_get_logger("terrain", {console_sink, file_sink}, pattern);
    g_logger_physics = quill::Frontend::create_or_get_logger("physics", {console_sink, file_sink}, pattern);
    g_logger_audio = quill::Frontend::create_or_get_logger("audio", {console_sink, file_sink}, pattern);
    g_logger_input = quill::Frontend::create_or_get_logger("input", {console_sink, file_sink}, pattern);
    g_logger_ui = quill::Frontend::create_or_get_logger("ui", {console_sink, file_sink}, pattern);
    g_logger_ecs = quill::Frontend::create_or_get_logger("ecs", {console_sink, file_sink}, pattern);
    g_logger_bgfx = quill::Frontend::create_or_get_logger("bgfx", {console_sink, file_sink}, pattern);

    // All loggers start at Info level
    for (auto logger : {g_logger_core, g_logger_render, g_logger_terrain, g_logger_physics, g_logger_audio,
                        g_logger_input, g_logger_ui, g_logger_ecs, g_logger_bgfx}) {
        if (logger)
            logger->set_log_level(quill::LogLevel::Info);
    }
}

void shutdown() {
    if (g_logger) {
        g_logger->flush_log();
    }
    // Flush all subsystem loggers
    for (auto logger : {g_logger_core, g_logger_render, g_logger_terrain, g_logger_physics, g_logger_audio,
                        g_logger_input, g_logger_ui, g_logger_ecs, g_logger_bgfx}) {
        if (logger)
            logger->flush_log();
    }
    quill::Backend::stop();
}

quill::Logger* logger() {
    return g_logger;
}

quill::Logger* coreLogger() {
    return g_logger_core;
}

quill::Logger* renderLogger() {
    return g_logger_render;
}

quill::Logger* terrainLogger() {
    return g_logger_terrain;
}

quill::Logger* physicsLogger() {
    return g_logger_physics;
}

quill::Logger* audioLogger() {
    return g_logger_audio;
}

quill::Logger* inputLogger() {
    return g_logger_input;
}

quill::Logger* uiLogger() {
    return g_logger_ui;
}

quill::Logger* ecsLogger() {
    return g_logger_ecs;
}

quill::Logger* bgfxLogger() {
    return g_logger_bgfx;
}

void setLevel(quill::LogLevel level) {
    if (g_logger) {
        g_logger->set_log_level(level);
    }
}

void setCoreLevel(quill::LogLevel level) {
    if (g_logger_core)
        g_logger_core->set_log_level(level);
}

void setRenderLevel(quill::LogLevel level) {
    if (g_logger_render)
        g_logger_render->set_log_level(level);
}

void setTerrainLevel(quill::LogLevel level) {
    if (g_logger_terrain)
        g_logger_terrain->set_log_level(level);
}

void setPhysicsLevel(quill::LogLevel level) {
    if (g_logger_physics)
        g_logger_physics->set_log_level(level);
}

void setAudioLevel(quill::LogLevel level) {
    if (g_logger_audio)
        g_logger_audio->set_log_level(level);
}

void setInputLevel(quill::LogLevel level) {
    if (g_logger_input)
        g_logger_input->set_log_level(level);
}

void setUILevel(quill::LogLevel level) {
    if (g_logger_ui)
        g_logger_ui->set_log_level(level);
}

void setECSLevel(quill::LogLevel level) {
    if (g_logger_ecs)
        g_logger_ecs->set_log_level(level);
}

void setBgfxLevel(quill::LogLevel level) {
    if (g_logger_bgfx)
        g_logger_bgfx->set_log_level(level);
}

} // namespace fabric::log
