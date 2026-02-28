#include "fabric/core/DataLoader.hh"

#include <fstream>
#include <sstream>

#include "fabric/core/Log.hh"

namespace fabric {

// -- DataLoader --

DataLoader::DataLoader(toml::table tbl, std::string source) : table_(std::move(tbl)), sourceName_(std::move(source)) {}

Result<DataLoader> DataLoader::load(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) {
        return Result<DataLoader>::error(ErrorCode::NotFound, "TOML file not found: " + path.string());
    }

    try {
        auto tbl = toml::parse_file(path.string());
        FABRIC_LOG_DEBUG("Loaded TOML: {}", path.string());
        return Result<DataLoader>::ok(DataLoader(std::move(tbl), path.string()));
    } catch (const toml::parse_error& err) {
        std::ostringstream oss;
        oss << path.string() << ":" << err.source().begin.line << ":" << err.source().begin.column << " - "
            << err.description();
        return Result<DataLoader>::error(ErrorCode::Internal, oss.str());
    }
}

Result<DataLoader> DataLoader::parse(std::string_view tomlContent, std::string_view sourceName) {
    try {
        auto tbl = toml::parse(tomlContent, sourceName);
        return Result<DataLoader>::ok(DataLoader(std::move(tbl), std::string(sourceName)));
    } catch (const toml::parse_error& err) {
        std::ostringstream oss;
        oss << sourceName << ":" << err.source().begin.line << ":" << err.source().begin.column << " - "
            << err.description();
        return Result<DataLoader>::error(ErrorCode::Internal, oss.str());
    }
}

const toml::node* DataLoader::resolve(std::string_view dottedKey) const {
    const toml::node* current = &table_;
    std::string_view remaining = dottedKey;

    while (!remaining.empty()) {
        auto dot = remaining.find('.');
        std::string_view segment = (dot == std::string_view::npos) ? remaining : remaining.substr(0, dot);

        if (!current->is_table()) {
            return nullptr;
        }
        current = current->as_table()->get(segment);
        if (!current) {
            return nullptr;
        }

        if (dot == std::string_view::npos) {
            break;
        }
        remaining = remaining.substr(dot + 1);
    }
    return current;
}

std::string DataLoader::formatError(std::string_view key, std::string_view expected) const {
    std::ostringstream oss;
    oss << sourceName_ << ": key '" << key << "' " << expected;
    return oss.str();
}

Result<std::string> DataLoader::getString(std::string_view key) const {
    const auto* node = resolve(key);
    if (!node) {
        return Result<std::string>::error(ErrorCode::NotFound, formatError(key, "not found"));
    }
    if (auto val = node->as_string()) {
        return Result<std::string>::ok(std::string(val->get()));
    }
    return Result<std::string>::error(ErrorCode::InvalidState, formatError(key, "is not a string"));
}

Result<int64_t> DataLoader::getInt(std::string_view key) const {
    const auto* node = resolve(key);
    if (!node) {
        return Result<int64_t>::error(ErrorCode::NotFound, formatError(key, "not found"));
    }
    if (auto val = node->as_integer()) {
        return Result<int64_t>::ok(val->get());
    }
    return Result<int64_t>::error(ErrorCode::InvalidState, formatError(key, "is not an integer"));
}

Result<double> DataLoader::getFloat(std::string_view key) const {
    const auto* node = resolve(key);
    if (!node) {
        return Result<double>::error(ErrorCode::NotFound, formatError(key, "not found"));
    }
    // Accept both float and integer values for float extraction
    if (auto val = node->as_floating_point()) {
        return Result<double>::ok(val->get());
    }
    if (auto val = node->as_integer()) {
        return Result<double>::ok(static_cast<double>(val->get()));
    }
    return Result<double>::error(ErrorCode::InvalidState, formatError(key, "is not a number"));
}

Result<bool> DataLoader::getBool(std::string_view key) const {
    const auto* node = resolve(key);
    if (!node) {
        return Result<bool>::error(ErrorCode::NotFound, formatError(key, "not found"));
    }
    if (auto val = node->as_boolean()) {
        return Result<bool>::ok(val->get());
    }
    return Result<bool>::error(ErrorCode::InvalidState, formatError(key, "is not a boolean"));
}

Result<std::vector<std::string>> DataLoader::getStringArray(std::string_view key) const {
    const auto* node = resolve(key);
    if (!node) {
        return Result<std::vector<std::string>>::error(ErrorCode::NotFound, formatError(key, "not found"));
    }
    if (auto arr = node->as_array()) {
        std::vector<std::string> result;
        result.reserve(arr->size());
        for (const auto& elem : *arr) {
            if (auto str = elem.as_string()) {
                result.emplace_back(str->get());
            } else {
                return Result<std::vector<std::string>>::error(ErrorCode::InvalidState,
                                                               formatError(key, "contains non-string element"));
            }
        }
        return Result<std::vector<std::string>>::ok(std::move(result));
    }
    return Result<std::vector<std::string>>::error(ErrorCode::InvalidState, formatError(key, "is not an array"));
}

bool DataLoader::hasKey(std::string_view key) const {
    return resolve(key) != nullptr;
}

const toml::table& DataLoader::table() const {
    return table_;
}

const std::string& DataLoader::sourceName() const {
    return sourceName_;
}

// -- DataRegistry --

Result<const DataLoader*> DataRegistry::get(const std::filesystem::path& path) {
    auto canonical = std::filesystem::absolute(path).string();

    std::lock_guard lock(mutex_);
    auto it = cache_.find(canonical);
    if (it != cache_.end()) {
        return Result<const DataLoader*>::ok(&it->second);
    }

    auto result = DataLoader::load(path);
    if (result.isError()) {
        return Result<const DataLoader*>::error(result.code(), result.message());
    }

    auto [inserted, _] = cache_.emplace(canonical, std::move(result.value()));
    return Result<const DataLoader*>::ok(&inserted->second);
}

Result<const DataLoader*> DataRegistry::reload(const std::filesystem::path& path) {
    auto canonical = std::filesystem::absolute(path).string();

    auto result = DataLoader::load(path);
    if (result.isError()) {
        return Result<const DataLoader*>::error(result.code(), result.message());
    }

    std::lock_guard lock(mutex_);
    cache_.erase(canonical);
    auto [it, _] = cache_.emplace(canonical, std::move(result.value()));
    return Result<const DataLoader*>::ok(&it->second);
}

void DataRegistry::remove(const std::filesystem::path& path) {
    auto canonical = std::filesystem::absolute(path).string();
    std::lock_guard lock(mutex_);
    cache_.erase(canonical);
}

void DataRegistry::clear() {
    std::lock_guard lock(mutex_);
    cache_.clear();
}

size_t DataRegistry::size() const {
    std::lock_guard lock(mutex_);
    return cache_.size();
}

} // namespace fabric
