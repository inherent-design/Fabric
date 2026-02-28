#pragma once

#include "fabric/utils/ErrorHandling.hh"

#include <toml++/toml.hpp>

#include <filesystem>
#include <functional>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace fabric {

// Typed extraction helpers that wrap toml++ node access with Result<T> error
// reporting. Each returns a descriptive error on type mismatch or missing key.
Result<std::string> getString(const toml::table& table, std::string_view key);
Result<int64_t> getInt(const toml::table& table, std::string_view key);
Result<double> getFloat(const toml::table& table, std::string_view key);
Result<bool> getBool(const toml::table& table, std::string_view key);
Result<const toml::table*> getTable(const toml::table& table, std::string_view key);
Result<const toml::array*> getArray(const toml::table& table, std::string_view key);

// Optional variants: return default value when key is absent, error only on
// type mismatch.
std::string getStringOr(const toml::table& table, std::string_view key, std::string_view defaultValue);
int64_t getIntOr(const toml::table& table, std::string_view key, int64_t defaultValue);
double getFloatOr(const toml::table& table, std::string_view key, double defaultValue);
bool getBoolOr(const toml::table& table, std::string_view key, bool defaultValue);

// Parses a TOML file from disk and returns the root table.
// Error messages include the file path and toml++ source location on failure.
Result<toml::table> parseTomlFile(const std::filesystem::path& path);

// Parses a TOML string (useful for testing without disk I/O).
Result<toml::table> parseTomlString(std::string_view content, std::string_view sourceName = "string");

// DataLoader: stateless parser that converts a TOML table into a typed struct.
// Users provide a deserialization function that maps toml::table -> T.
//
// Usage:
//   auto result = DataLoader::load<ItemDef>("data/schema/item.toml",
//       [](const toml::table& t) -> Result<ItemDef> {
//           auto id = getString(t, "id");
//           if (id.isError()) return Result<ItemDef>::error(id.code(), id.message());
//           // ... build ItemDef ...
//       });
class DataLoader {
  public:
    template <typename T> using Deserializer = std::function<Result<T>(const toml::table&)>;

    // Load a TOML file and convert it to T via the provided deserializer.
    template <typename T> static Result<T> load(const std::filesystem::path& path, Deserializer<T> deserializer) {
        auto parsed = parseTomlFile(path);
        if (parsed.isError()) {
            return Result<T>::error(parsed.code(), parsed.message());
        }
        return deserializer(parsed.value());
    }

    // Load all items from an array-of-tables TOML file.
    // Expects the file to contain [[arrayKey]] entries.
    template <typename T>
    static Result<std::vector<T>> loadAll(const std::filesystem::path& path, std::string_view arrayKey,
                                          Deserializer<T> deserializer) {
        auto parsed = parseTomlFile(path);
        if (parsed.isError()) {
            return Result<std::vector<T>>::error(parsed.code(), parsed.message());
        }

        auto* arr = parsed.value().get_as<toml::array>(arrayKey);
        if (!arr) {
            return Result<std::vector<T>>::error(ErrorCode::NotFound,
                                                 std::string("missing array key '") + std::string(arrayKey) + "'");
        }

        std::vector<T> items;
        items.reserve(arr->size());

        for (std::size_t i = 0; i < arr->size(); ++i) {
            auto* tbl = arr->at(i).as_table();
            if (!tbl) {
                return Result<std::vector<T>>::error(ErrorCode::InvalidState,
                                                     std::string("element ") + std::to_string(i) + " in '" +
                                                         std::string(arrayKey) + "' is not a table");
            }
            auto item = deserializer(*tbl);
            if (item.isError()) {
                return Result<std::vector<T>>::error(item.code(), std::string("element ") + std::to_string(i) + ": " +
                                                                      item.message());
            }
            items.push_back(std::move(item.value()));
        }

        return Result<std::vector<T>>::ok(std::move(items));
    }
};

// DataRegistry: thread-safe cache of parsed TOML tables keyed by file path.
// Integration point for hot-reload (EF-8): FileWatcher triggers reload().
class DataRegistry {
  public:
    // Get or load a TOML table for the given path. Caches the result.
    Result<const toml::table*> get(const std::filesystem::path& path);

    // Invalidate cache for a specific path and re-parse from disk.
    Result<const toml::table*> reload(const std::filesystem::path& path);

    // Invalidate all cached entries.
    void clear();

    // Check whether a path is currently cached.
    bool contains(const std::filesystem::path& path) const;

    // Number of cached entries.
    std::size_t size() const;

  private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, toml::table> cache_;
};

} // namespace fabric
