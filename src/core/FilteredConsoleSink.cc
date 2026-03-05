#include "fabric/core/FilteredConsoleSink.hh"

#include <algorithm>
#include <string>

namespace fabric::log {

FilteredConsoleSink::FilteredConsoleSink() : inner_(std::make_shared<quill::ConsoleSink>()) {}

FilteredConsoleSink::FilteredConsoleSink(quill::ConsoleSinkConfig const& config)
    : inner_(std::make_shared<quill::ConsoleSink>(config)) {}

void FilteredConsoleSink::setIncludePatterns(std::vector<std::string> patterns) {
    include_patterns_ = std::move(patterns);
}

void FilteredConsoleSink::setExcludePatterns(std::vector<std::string> patterns) {
    exclude_patterns_ = std::move(patterns);
}

void FilteredConsoleSink::write_log(quill::MacroMetadata const* log_metadata, uint64_t log_timestamp,
                                    std::string_view thread_id, std::string_view thread_name,
                                    std::string const& process_id, std::string_view logger_name,
                                    quill::LogLevel log_level, std::string_view log_level_description,
                                    std::string_view log_level_short_code,
                                    std::vector<std::pair<std::string, std::string>> const* named_args,
                                    std::string_view log_message, std::string_view log_statement) {

    // Check if this logger should be logged
    if (!shouldLog(std::string(logger_name))) {
        return; // Filtered out
    }

    // Pass through to inner console sink
    inner_->write_log(log_metadata, log_timestamp, thread_id, thread_name, process_id, logger_name, log_level,
                      log_level_description, log_level_short_code, named_args, log_message, log_statement);
}

void FilteredConsoleSink::flush_sink() {
    inner_->flush_sink();
}

bool FilteredConsoleSink::shouldLog(const std::string& loggerName) const {
    // 1. Check exclude patterns first (highest priority)
    for (const auto& pattern : exclude_patterns_) {
        if (matchesPattern(loggerName, pattern)) {
            return false;
        }
    }

    // 2. If no include patterns, allow everything not excluded
    if (include_patterns_.empty()) {
        return true;
    }

    // 3. Check include patterns
    for (const auto& pattern : include_patterns_) {
        if (matchesPattern(loggerName, pattern)) {
            return true;
        }
    }

    return false;
}

bool FilteredConsoleSink::matchesPattern(const std::string& name, const std::string& pattern) const {
    // Handle wildcard suffix: "fabric.*"
    if (pattern.size() >= 2 && pattern.substr(pattern.size() - 2) == ".*") {
        std::string prefix = pattern.substr(0, pattern.size() - 2);
        // Match exact or prefix with dot
        if (name == prefix)
            return true;
        if (name.size() > prefix.size() && name.substr(0, prefix.size()) == prefix && name[prefix.size()] == '.') {
            return true;
        }
        return false;
    }

    // Single wildcard: "*"
    if (pattern == "*") {
        return true;
    }

    // Exact match
    return name == pattern;
}

} // namespace fabric::log
