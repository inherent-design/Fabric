#pragma once

#include <filesystem>
#include <functional>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>

#include <toml++/toml.hpp>

#include "fabric/fx/Error.hh"
#include "fabric/fx/Result.hh"

namespace recurse {

using fabric::fx::IOError;
using fabric::fx::NotFound;
using fabric::fx::Result;
using fabric::fx::StateError;

/// Wraps a parsed TOML table with typed extraction helpers.
class DataLoader {
  public:
    static Result<DataLoader, NotFound, IOError> load(const std::filesystem::path& path);

    static Result<DataLoader, IOError> parse(std::string_view tomlContent, std::string_view sourceName = "<string>");

    Result<std::string, NotFound, StateError> getString(std::string_view key) const;
    Result<int64_t, NotFound, StateError> getInt(std::string_view key) const;
    Result<double, NotFound, StateError> getFloat(std::string_view key) const;
    Result<bool, NotFound, StateError> getBool(std::string_view key) const;

    Result<std::vector<std::string>, NotFound, StateError> getStringArray(std::string_view key) const;

    bool hasKey(std::string_view key) const;

    const toml::table& table() const;

    const std::string& sourceName() const;

  private:
    DataLoader(toml::table tbl, std::string source);

    const toml::node* resolve(std::string_view dottedKey) const;

    std::string formatError(std::string_view key, std::string_view expected) const;

    toml::table table_;
    std::string sourceName_;
};

/// Caches parsed DataLoader instances by file path. Thread-safe.
class DataRegistry {
  public:
    Result<const DataLoader*, NotFound, IOError> get(const std::filesystem::path& path);

    Result<const DataLoader*, NotFound, IOError> reload(const std::filesystem::path& path);

    void remove(const std::filesystem::path& path);

    void clear();

    size_t size() const;

  private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, DataLoader> cache_;
};

} // namespace recurse
