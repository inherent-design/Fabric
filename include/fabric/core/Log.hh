#pragma once

// Fabric Engine Logging Subsystem
// Wraps Quill v11.x async structured logging.
//
// Usage:
//   #include "fabric/core/Log.hh"
//   FABRIC_LOG_INFO("Server started on port {}", port);
//   FABRIC_LOG_ERROR("Failed to load resource: {}", resource_id);

#include <quill/Backend.h>
#include <quill/Frontend.h>
#include <quill/LogMacros.h>
#include <quill/Logger.h>

namespace fabric::log {

/// Initialize the logging subsystem (console output only).
/// Call once at startup before any logging.
void init();

/// Initialize with file sink in addition to console.
void init(const char* log_file_path);

/// Flush pending messages and stop the backend thread.
void shutdown();

/// Get the root logger. Valid after init().
quill::Logger* logger();

/// Set runtime log level (within compile-time ceiling).
void setLevel(quill::LogLevel level);

} // namespace fabric::log

// Fabric logging macros - wrap Quill with the root logger.
// Compile-time filtering: in Release builds, DEBUG and TRACE are absent.
#define FABRIC_LOG_TRACE(fmt, ...)    QUILL_LOG_TRACE_L1(fabric::log::logger(), fmt, ##__VA_ARGS__)
#define FABRIC_LOG_DEBUG(fmt, ...)    QUILL_LOG_DEBUG(fabric::log::logger(), fmt, ##__VA_ARGS__)
#define FABRIC_LOG_INFO(fmt, ...)     QUILL_LOG_INFO(fabric::log::logger(), fmt, ##__VA_ARGS__)
#define FABRIC_LOG_WARN(fmt, ...)     QUILL_LOG_WARNING(fabric::log::logger(), fmt, ##__VA_ARGS__)
#define FABRIC_LOG_ERROR(fmt, ...)    QUILL_LOG_ERROR(fabric::log::logger(), fmt, ##__VA_ARGS__)
#define FABRIC_LOG_CRITICAL(fmt, ...) QUILL_LOG_CRITICAL(fabric::log::logger(), fmt, ##__VA_ARGS__)
