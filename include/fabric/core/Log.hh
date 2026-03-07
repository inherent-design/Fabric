#pragma once

// Fabric Engine Logging Subsystem
// Wraps Quill v11.x async structured logging.
//
// Usage:
//   #include "fabric/core/Log.hh"
//   FABRIC_LOG_INFO("Server started on port {}", port);
//   FABRIC_LOG_ERROR("Failed to load resource: {}", resource_id);

#include "fabric/core/LogConfig.hh"

// Neutralize X11 macro pollution.  <X11/X.h> (included transitively by
// WebKitGTK and other Linux system headers) defines bare-word macros that
// collide with Quill's enum member names (e.g. Always, None, Never).
// Undefining them here keeps the Quill headers parseable regardless of
// include order.
#ifdef Always
#undef Always
#endif
#ifdef None
#undef None
#endif
#ifdef Never
#undef Never
#endif
#ifdef Bool
#undef Bool
#endif
#ifdef Status
#undef Status
#endif
#ifdef Success
#undef Success
#endif
#ifdef True
#undef True
#endif
#ifdef False
#undef False
#endif

#include <quill/Backend.h>
#include <quill/Frontend.h>
#include <quill/Logger.h>
#include <quill/LogMacros.h>

namespace fabric::log {

/// Initialize the logging subsystem (console output only).
/// Call once at startup before any logging.
void init();

/// Initialize with file sink in addition to console.
void init(const char* logFilePath);

/// Initialize with full LogConfig (recommended).
/// Supports per-run folders, per-logger levels, and console filtering.
void init(const LogConfig& config);

/// Flush pending messages and stop the backend thread.
void shutdown();

/// Get the root logger. Safe to call before init() (lazy-inits a
/// console-only fallback so the returned pointer is never null).
quill::Logger* logger();

/// Get per-subsystem loggers. Valid after init().
quill::Logger* renderLogger();
quill::Logger* physicsLogger();
quill::Logger* audioLogger();
quill::Logger* bgfxLogger();

/// Set runtime log level for root logger (within compile-time ceiling).
void setLevel(quill::LogLevel level);

/// Set runtime log levels per subsystem.
void setRenderLevel(quill::LogLevel level);
void setPhysicsLevel(quill::LogLevel level);
void setAudioLevel(quill::LogLevel level);
void setBgfxLevel(quill::LogLevel level);

} // namespace fabric::log

// Root logger macros - wrap Quill with the root logger.
// Compile-time filtering: in Release builds, DEBUG and TRACE are absent.
#define FABRIC_LOG_TRACE(fmt, ...) QUILL_LOG_TRACE_L1(fabric::log::logger(), fmt, ##__VA_ARGS__)
#define FABRIC_LOG_DEBUG(fmt, ...) QUILL_LOG_DEBUG(fabric::log::logger(), fmt, ##__VA_ARGS__)
#define FABRIC_LOG_INFO(fmt, ...) QUILL_LOG_INFO(fabric::log::logger(), fmt, ##__VA_ARGS__)
#define FABRIC_LOG_WARN(fmt, ...) QUILL_LOG_WARNING(fabric::log::logger(), fmt, ##__VA_ARGS__)
#define FABRIC_LOG_ERROR(fmt, ...) QUILL_LOG_ERROR(fabric::log::logger(), fmt, ##__VA_ARGS__)
#define FABRIC_LOG_CRITICAL(fmt, ...) QUILL_LOG_CRITICAL(fabric::log::logger(), fmt, ##__VA_ARGS__)

// Render subsystem macros (frame perf, draw calls, GPU stats, shader init)
#define FABRIC_LOG_RENDER_TRACE(fmt, ...) QUILL_LOG_TRACE_L1(fabric::log::renderLogger(), fmt, ##__VA_ARGS__)
#define FABRIC_LOG_RENDER_DEBUG(fmt, ...) QUILL_LOG_DEBUG(fabric::log::renderLogger(), fmt, ##__VA_ARGS__)
#define FABRIC_LOG_RENDER_INFO(fmt, ...) QUILL_LOG_INFO(fabric::log::renderLogger(), fmt, ##__VA_ARGS__)
#define FABRIC_LOG_RENDER_WARN(fmt, ...) QUILL_LOG_WARNING(fabric::log::renderLogger(), fmt, ##__VA_ARGS__)
#define FABRIC_LOG_RENDER_ERROR(fmt, ...) QUILL_LOG_ERROR(fabric::log::renderLogger(), fmt, ##__VA_ARGS__)
#define FABRIC_LOG_RENDER_CRITICAL(fmt, ...) QUILL_LOG_CRITICAL(fabric::log::renderLogger(), fmt, ##__VA_ARGS__)

// Physics subsystem macros (body count, collision events)
#define FABRIC_LOG_PHYSICS_TRACE(fmt, ...) QUILL_LOG_TRACE_L1(fabric::log::physicsLogger(), fmt, ##__VA_ARGS__)
#define FABRIC_LOG_PHYSICS_DEBUG(fmt, ...) QUILL_LOG_DEBUG(fabric::log::physicsLogger(), fmt, ##__VA_ARGS__)
#define FABRIC_LOG_PHYSICS_INFO(fmt, ...) QUILL_LOG_INFO(fabric::log::physicsLogger(), fmt, ##__VA_ARGS__)
#define FABRIC_LOG_PHYSICS_WARN(fmt, ...) QUILL_LOG_WARNING(fabric::log::physicsLogger(), fmt, ##__VA_ARGS__)
#define FABRIC_LOG_PHYSICS_ERROR(fmt, ...) QUILL_LOG_ERROR(fabric::log::physicsLogger(), fmt, ##__VA_ARGS__)
#define FABRIC_LOG_PHYSICS_CRITICAL(fmt, ...) QUILL_LOG_CRITICAL(fabric::log::physicsLogger(), fmt, ##__VA_ARGS__)

// Audio subsystem macros (voice count, listener updates)
#define FABRIC_LOG_AUDIO_TRACE(fmt, ...) QUILL_LOG_TRACE_L1(fabric::log::audioLogger(), fmt, ##__VA_ARGS__)
#define FABRIC_LOG_AUDIO_DEBUG(fmt, ...) QUILL_LOG_DEBUG(fabric::log::audioLogger(), fmt, ##__VA_ARGS__)
#define FABRIC_LOG_AUDIO_INFO(fmt, ...) QUILL_LOG_INFO(fabric::log::audioLogger(), fmt, ##__VA_ARGS__)
#define FABRIC_LOG_AUDIO_WARN(fmt, ...) QUILL_LOG_WARNING(fabric::log::audioLogger(), fmt, ##__VA_ARGS__)
#define FABRIC_LOG_AUDIO_ERROR(fmt, ...) QUILL_LOG_ERROR(fabric::log::audioLogger(), fmt, ##__VA_ARGS__)
#define FABRIC_LOG_AUDIO_CRITICAL(fmt, ...) QUILL_LOG_CRITICAL(fabric::log::audioLogger(), fmt, ##__VA_ARGS__)

// bgfx subsystem macros (bgfx internal messages via callback)
#define FABRIC_LOG_BGFX_TRACE(fmt, ...) QUILL_LOG_TRACE_L1(fabric::log::bgfxLogger(), fmt, ##__VA_ARGS__)
#define FABRIC_LOG_BGFX_DEBUG(fmt, ...) QUILL_LOG_DEBUG(fabric::log::bgfxLogger(), fmt, ##__VA_ARGS__)
#define FABRIC_LOG_BGFX_INFO(fmt, ...) QUILL_LOG_INFO(fabric::log::bgfxLogger(), fmt, ##__VA_ARGS__)
#define FABRIC_LOG_BGFX_WARN(fmt, ...) QUILL_LOG_WARNING(fabric::log::bgfxLogger(), fmt, ##__VA_ARGS__)
#define FABRIC_LOG_BGFX_ERROR(fmt, ...) QUILL_LOG_ERROR(fabric::log::bgfxLogger(), fmt, ##__VA_ARGS__)
#define FABRIC_LOG_BGFX_CRITICAL(fmt, ...) QUILL_LOG_CRITICAL(fabric::log::bgfxLogger(), fmt, ##__VA_ARGS__)
