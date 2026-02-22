#include "fabric/core/Log.hh"

#include <quill/Backend.h>
#include <quill/Frontend.h>
#include <quill/Logger.h>
#include <quill/sinks/ConsoleSink.h>
#include <quill/sinks/FileSink.h>

namespace fabric::log {

namespace {
  quill::Logger* g_logger = nullptr;
}

void init() {
  quill::BackendOptions backend_opts;
  backend_opts.thread_name = "FabricLog";
  backend_opts.wait_for_queues_to_empty_before_exit = true;

  quill::Backend::start(backend_opts);

  auto console_sink =
    quill::Frontend::create_or_get_sink<quill::ConsoleSink>("console");

  quill::PatternFormatterOptions pattern;
  pattern.format_pattern =
    "%(time) [%(thread_id)] %(short_source_location:<28) "
    "%(log_level:<9) %(message)";
  pattern.timestamp_pattern = "%H:%M:%S.%Qms";

  g_logger = quill::Frontend::create_or_get_logger(
    "fabric", std::move(console_sink), pattern);
  g_logger->set_log_level(quill::LogLevel::Info);
}

void init(const char* log_file_path) {
  quill::BackendOptions backend_opts;
  backend_opts.thread_name = "FabricLog";
  backend_opts.wait_for_queues_to_empty_before_exit = true;

  quill::Backend::start(backend_opts);

  auto console_sink =
    quill::Frontend::create_or_get_sink<quill::ConsoleSink>("console");

  auto file_sink = quill::Frontend::create_or_get_sink<quill::FileSink>(
    log_file_path,
    []() {
      quill::FileSinkConfig cfg;
      cfg.set_open_mode('w');
      cfg.set_filename_append_option(
        quill::FilenameAppendOption::StartDateTime);
      return cfg;
    }());

  quill::PatternFormatterOptions pattern;
  pattern.format_pattern =
    "%(time) [%(thread_id)] %(short_source_location:<28) "
    "%(log_level:<9) %(message)";
  pattern.timestamp_pattern = "%H:%M:%S.%Qms";

  g_logger = quill::Frontend::create_or_get_logger(
    "fabric", {console_sink, file_sink}, pattern);
  g_logger->set_log_level(quill::LogLevel::Info);
}

void shutdown() {
  if (g_logger) {
    g_logger->flush_log();
  }
  quill::Backend::stop();
}

quill::Logger* logger() {
  return g_logger;
}

void setLevel(quill::LogLevel level) {
  if (g_logger) {
    g_logger->set_log_level(level);
  }
}

} // namespace fabric::log
