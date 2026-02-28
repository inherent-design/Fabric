#pragma once

#include <filesystem>
#include <functional>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>

#include <toml++/toml.hpp>

#include "fabric/utils/ErrorHandling.hh"

namespace fabric {

// Wraps a parsed TOML table with typed extraction helpers.
// Each accessor returns Result<T> so callers get file:key context on failure.
class DataLoader {
  public:
    // Parse a .toml file from disk. Returns error with path context on failure.
    static Result<DataLoader> load(const std::filesystem::path& path);

    // Parse a TOML string directly (useful for tests).
    static Result<DataLoader> parse(std::string_view tomlContent, std::string_view sourceName = "<string>");

    // Typed value extraction with dotted-key paths (e.g., "item.stats.damage").
    Result<std::string> getString(std::string_view key) const;
    Result<int64_t> getInt(std::string_view key) const;
    Result<double> getFloat(std::string_view key) const;
    Result<bool> getBool(std::string_view key) const;

    // Array extraction
    Result<std::vector<std::string>> getStringArray(std::string_view key) const;

    // Check if a key exists (dotted path)
    bool hasKey(std::string_view key) const;

    // Access the underlying toml table
    const toml::table& table() const;

    // Source path (file path or "<string>" for inline parsing)
    const std::string& sourceName() const;

  private:
    DataLoader(toml::table tbl, std::string source);

    // Navigate a dotted key path, returning the target node or nullptr
    const toml::node* resolve(std::string_view dottedKey) const;

    // Format an error message with source context
    std::string formatError(std::string_view key, std::string_view expected) const;

    toml::table table_;
    std::string sourceName_;
};

// Caches parsed DataLoader instances by file path. Thread-safe.
// Provides reload() to invalidate a cached entry for hot-reload integration.
class DataRegistry {
  public:
    // Get or load a DataLoader for the given path. Caches on first access.
    Result<const DataLoader*> get(const std::filesystem::path& path);

    // Invalidate and re-parse a cached path. Returns error if re-parse fails.
    Result<const DataLoader*> reload(const std::filesystem::path& path);

    // Remove a path from the cache
    void remove(const std::filesystem::path& path);

    // Clear all cached entries
    void clear();

    // Number of cached entries
    size_t size() const;

  private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, DataLoader> cache_;
};

} // namespace fabric
