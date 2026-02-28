#include "fabric/core/DataLoader.hh"
#include "fabric/core/Log.hh"

#include <fstream>
#include <sstream>

namespace fabric {

// -- Typed extraction helpers ------------------------------------------------

Result<std::string> getString(const toml::table& table, std::string_view key) {
    auto node = table[key];
    if (!node) {
        return Result<std::string>::error(ErrorCode::NotFound,
                                          std::string("missing required key '") + std::string(key) + "'");
    }
    auto val = node.value<std::string>();
    if (!val) {
        return Result<std::string>::error(ErrorCode::InvalidState,
                                          std::string("key '") + std::string(key) + "' is not a string");
    }
    return Result<std::string>::ok(std::move(*val));
}

Result<int64_t> getInt(const toml::table& table, std::string_view key) {
    auto node = table[key];
    if (!node) {
        return Result<int64_t>::error(ErrorCode::NotFound,
                                      std::string("missing required key '") + std::string(key) + "'");
    }
    auto val = node.value<int64_t>();
    if (!val) {
        return Result<int64_t>::error(ErrorCode::InvalidState,
                                      std::string("key '") + std::string(key) + "' is not an integer");
    }
    return Result<int64_t>::ok(*val);
}

Result<double> getFloat(const toml::table& table, std::string_view key) {
    auto node = table[key];
    if (!node) {
        return Result<double>::error(ErrorCode::NotFound,
                                     std::string("missing required key '") + std::string(key) + "'");
    }
    // Accept both integer and float values, coerce integers to double
    auto floatVal = node.value<double>();
    if (floatVal) {
        return Result<double>::ok(*floatVal);
    }
    auto intVal = node.value<int64_t>();
    if (intVal) {
        return Result<double>::ok(static_cast<double>(*intVal));
    }
    return Result<double>::error(ErrorCode::InvalidState,
                                 std::string("key '") + std::string(key) + "' is not a number");
}

Result<bool> getBool(const toml::table& table, std::string_view key) {
    auto node = table[key];
    if (!node) {
        return Result<bool>::error(ErrorCode::NotFound, std::string("missing required key '") + std::string(key) + "'");
    }
    auto val = node.value<bool>();
    if (!val) {
        return Result<bool>::error(ErrorCode::InvalidState,
                                   std::string("key '") + std::string(key) + "' is not a boolean");
    }
    return Result<bool>::ok(*val);
}

Result<const toml::table*> getTable(const toml::table& table, std::string_view key) {
    auto node = table[key];
    if (!node) {
        return Result<const toml::table*>::error(ErrorCode::NotFound,
                                                 std::string("missing required key '") + std::string(key) + "'");
    }
    auto* tbl = node.as_table();
    if (!tbl) {
        return Result<const toml::table*>::error(ErrorCode::InvalidState,
                                                 std::string("key '") + std::string(key) + "' is not a table");
    }
    return Result<const toml::table*>::ok(tbl);
}

Result<const toml::array*> getArray(const toml::table& table, std::string_view key) {
    auto node = table[key];
    if (!node) {
        return Result<const toml::array*>::error(ErrorCode::NotFound,
                                                 std::string("missing required key '") + std::string(key) + "'");
    }
    auto* arr = node.as_array();
    if (!arr) {
        return Result<const toml::array*>::error(ErrorCode::InvalidState,
                                                 std::string("key '") + std::string(key) + "' is not an array");
    }
    return Result<const toml::array*>::ok(arr);
}

// -- Optional variants -------------------------------------------------------

std::string getStringOr(const toml::table& table, std::string_view key, std::string_view defaultValue) {
    auto node = table[key];
    if (!node)
        return std::string(defaultValue);
    auto val = node.value<std::string>();
    return val ? std::move(*val) : std::string(defaultValue);
}

int64_t getIntOr(const toml::table& table, std::string_view key, int64_t defaultValue) {
    auto node = table[key];
    if (!node)
        return defaultValue;
    return node.value<int64_t>().value_or(defaultValue);
}

double getFloatOr(const toml::table& table, std::string_view key, double defaultValue) {
    auto node = table[key];
    if (!node)
        return defaultValue;
    auto floatVal = node.value<double>();
    if (floatVal)
        return *floatVal;
    auto intVal = node.value<int64_t>();
    if (intVal)
        return static_cast<double>(*intVal);
    return defaultValue;
}

bool getBoolOr(const toml::table& table, std::string_view key, bool defaultValue) {
    auto node = table[key];
    if (!node)
        return defaultValue;
    return node.value<bool>().value_or(defaultValue);
}

// -- Parsing -----------------------------------------------------------------

Result<toml::table> parseTomlFile(const std::filesystem::path& path) {
    auto pathStr = path.string();

    if (!std::filesystem::exists(path)) {
        FABRIC_LOG_ERROR("TOML file not found: {}", pathStr);
        return Result<toml::table>::error(ErrorCode::NotFound, "file not found: " + pathStr);
    }

    try {
        auto result = toml::parse_file(pathStr);
        FABRIC_LOG_DEBUG("Parsed TOML file: {}", pathStr);
        return Result<toml::table>::ok(std::move(result));
    } catch (const toml::parse_error& err) {
        std::ostringstream oss;
        oss << err;
        auto msg = pathStr + ": " + oss.str();
        FABRIC_LOG_ERROR("TOML parse error: {}", msg);
        return Result<toml::table>::error(ErrorCode::InvalidState, msg);
    }
}

Result<toml::table> parseTomlString(std::string_view content, std::string_view sourceName) {
    try {
        auto result = toml::parse(content, sourceName);
        return Result<toml::table>::ok(std::move(result));
    } catch (const toml::parse_error& err) {
        std::ostringstream oss;
        oss << err;
        auto msg = std::string(sourceName) + ": " + oss.str();
        FABRIC_LOG_ERROR("TOML parse error: {}", msg);
        return Result<toml::table>::error(ErrorCode::InvalidState, msg);
    }
}

// -- DataRegistry ------------------------------------------------------------

Result<const toml::table*> DataRegistry::get(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) {
        return Result<const toml::table*>::error(ErrorCode::NotFound, "file not found: " + path.string());
    }

    auto key = std::filesystem::canonical(path).string();

    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = cache_.find(key);
        if (it != cache_.end()) {
            return Result<const toml::table*>::ok(&it->second);
        }
    }

    auto parsed = parseTomlFile(path);
    if (parsed.isError()) {
        return Result<const toml::table*>::error(parsed.code(), parsed.message());
    }

    std::lock_guard<std::mutex> lock(mutex_);
    auto [inserted, _] = cache_.emplace(key, std::move(parsed.value()));
    return Result<const toml::table*>::ok(&inserted->second);
}

Result<const toml::table*> DataRegistry::reload(const std::filesystem::path& path) {
    auto key = std::filesystem::canonical(path).string();

    auto parsed = parseTomlFile(path);
    if (parsed.isError()) {
        return Result<const toml::table*>::error(parsed.code(), parsed.message());
    }

    std::lock_guard<std::mutex> lock(mutex_);

    cache_[key] = std::move(parsed.value());
    return Result<const toml::table*>::ok(&cache_[key]);
}

void DataRegistry::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    cache_.clear();
}

bool DataRegistry::contains(const std::filesystem::path& path) const {
    std::lock_guard<std::mutex> lock(mutex_);
    try {
        auto key = std::filesystem::canonical(path).string();
        return cache_.count(key) > 0;
    } catch (...) {
        return false;
    }
}

std::size_t DataRegistry::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return cache_.size();
}

} // namespace fabric
