#pragma once

// Fabric Engine Filtered Console Sink
// Custom Quill sink that wraps ConsoleSink with pattern-based filtering.
// Enables include/exclude patterns for logger names (e.g., "fabric.*", "bgfx.*").

#include <quill/sinks/ConsoleSink.h>
#include <quill/sinks/Sink.h>

#include <memory>
#include <string>
#include <vector>

namespace fabric::log {

/// Console sink with pattern-based filtering.
/// Wraps a standard ConsoleSink and filters log messages based on
/// include/exclude patterns matched against logger names.
///
/// Pattern syntax:
///   "fabric"      - Exact match
///   "fabric.*"    - Matches "fabric", "fabric.render", "fabric.physics"
///   "*"           - Matches everything
///
/// Filtering logic:
///   1. If logger matches any exclude pattern -> SKIP
///   2. If include patterns empty -> ALLOW
///   3. If logger matches any include pattern -> ALLOW
///   4. Otherwise -> SKIP
class FilteredConsoleSink : public quill::Sink {
  public:
    /// Construct a filtered console sink with default ConsoleSinkConfig
    FilteredConsoleSink();

    /// Construct with custom ConsoleSinkConfig
    explicit FilteredConsoleSink(quill::ConsoleSinkConfig const& config);

    ~FilteredConsoleSink() override = default;

    FilteredConsoleSink(FilteredConsoleSink const&) = delete;
    FilteredConsoleSink& operator=(FilteredConsoleSink const&) = delete;

    /// Set patterns for loggers to INCLUDE (empty = include all not excluded)
    void setIncludePatterns(std::vector<std::string> patterns);

    /// Set patterns for loggers to EXCLUDE (takes precedence over include)
    void setExcludePatterns(std::vector<std::string> patterns);

    /// Set minimum log level for console output.
    /// Messages below this level are suppressed even if the logger-level
    /// allows them (needed when file_level < console_level).
    void setMinLevel(quill::LogLevel level);

    /// Write log if it passes filters
    /// Overrides quill::Sink::write_log
    void write_log(quill::MacroMetadata const* log_metadata, uint64_t log_timestamp, std::string_view thread_id,
                   std::string_view thread_name, std::string const& process_id, std::string_view logger_name,
                   quill::LogLevel log_level, std::string_view log_level_description,
                   std::string_view log_level_short_code,
                   std::vector<std::pair<std::string, std::string>> const* named_args, std::string_view log_message,
                   std::string_view log_statement) override;

    /// Passthrough to inner sink
    void flush_sink() override;

  private:
    /// Check if logger name matches pattern (supports * wildcard)
    bool matchesPattern(const std::string& loggerName, const std::string& pattern) const;

    /// Check if logger should be logged (passes filters)
    bool shouldLog(const std::string& loggerName) const;

    std::shared_ptr<quill::ConsoleSink> inner_;
    std::vector<std::string> include_patterns_;
    std::vector<std::string> exclude_patterns_;
    quill::LogLevel minLevel_{quill::LogLevel::Info};
};

} // namespace fabric::log
