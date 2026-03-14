#include "recurse/persistence/DataLoader.hh"

#include <fstream>
#include <sstream>

#include "fabric/core/Log.hh"

namespace recurse {

using fabric::fx::ErrorContext;
using LoadResult = Result<DataLoader, NotFound, IOError>;
using ParseResult = Result<DataLoader, IOError>;

DataLoader::DataLoader(toml::table tbl, std::string source) : table_(std::move(tbl)), sourceName_(std::move(source)) {}

LoadResult DataLoader::load(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) {
        return LoadResult::failure(NotFound{path.string(), ErrorContext{"TOML file not found: " + path.string()}});
    }

    try {
        auto tbl = toml::parse_file(path.string());
        FABRIC_LOG_DEBUG("Loaded TOML: {}", path.string());
        return LoadResult::success(DataLoader(std::move(tbl), path.string()));
    } catch (const toml::parse_error& err) {
        std::ostringstream oss;
        oss << path.string() << ":" << err.source().begin.line << ":" << err.source().begin.column << " - "
            << err.description();
        return LoadResult::failure(IOError{path.string(), 0, ErrorContext{oss.str()}});
    }
}

ParseResult DataLoader::parse(std::string_view tomlContent, std::string_view sourceName) {
    try {
        auto tbl = toml::parse(tomlContent, sourceName);
        return ParseResult::success(DataLoader(std::move(tbl), std::string(sourceName)));
    } catch (const toml::parse_error& err) {
        std::ostringstream oss;
        oss << sourceName << ":" << err.source().begin.line << ":" << err.source().begin.column << " - "
            << err.description();
        return ParseResult::failure(IOError{std::string(sourceName), 0, ErrorContext{oss.str()}});
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

Result<std::string, NotFound, StateError> DataLoader::getString(std::string_view key) const {
    using R = Result<std::string, NotFound, StateError>;
    const auto* node = resolve(key);
    if (!node) {
        return R::failure(NotFound{std::string(key), ErrorContext{formatError(key, "not found")}});
    }
    if (auto val = node->as_string()) {
        return R::success(std::string(val->get()));
    }
    return R::failure(StateError{"string", "other", ErrorContext{formatError(key, "is not a string")}});
}

Result<int64_t, NotFound, StateError> DataLoader::getInt(std::string_view key) const {
    using R = Result<int64_t, NotFound, StateError>;
    const auto* node = resolve(key);
    if (!node) {
        return R::failure(NotFound{std::string(key), ErrorContext{formatError(key, "not found")}});
    }
    if (auto val = node->as_integer()) {
        return R::success(val->get());
    }
    return R::failure(StateError{"integer", "other", ErrorContext{formatError(key, "is not an integer")}});
}

Result<double, NotFound, StateError> DataLoader::getFloat(std::string_view key) const {
    using R = Result<double, NotFound, StateError>;
    const auto* node = resolve(key);
    if (!node) {
        return R::failure(NotFound{std::string(key), ErrorContext{formatError(key, "not found")}});
    }
    if (auto val = node->as_floating_point()) {
        return R::success(val->get());
    }
    if (auto val = node->as_integer()) {
        return R::success(static_cast<double>(val->get()));
    }
    return R::failure(StateError{"number", "other", ErrorContext{formatError(key, "is not a number")}});
}

Result<bool, NotFound, StateError> DataLoader::getBool(std::string_view key) const {
    using R = Result<bool, NotFound, StateError>;
    const auto* node = resolve(key);
    if (!node) {
        return R::failure(NotFound{std::string(key), ErrorContext{formatError(key, "not found")}});
    }
    if (auto val = node->as_boolean()) {
        return R::success(val->get());
    }
    return R::failure(StateError{"boolean", "other", ErrorContext{formatError(key, "is not a boolean")}});
}

Result<std::vector<std::string>, NotFound, StateError> DataLoader::getStringArray(std::string_view key) const {
    using R = Result<std::vector<std::string>, NotFound, StateError>;
    const auto* node = resolve(key);
    if (!node) {
        return R::failure(NotFound{std::string(key), ErrorContext{formatError(key, "not found")}});
    }
    if (auto arr = node->as_array()) {
        std::vector<std::string> result;
        result.reserve(arr->size());
        for (const auto& elem : *arr) {
            if (auto str = elem.as_string()) {
                result.emplace_back(str->get());
            } else {
                return R::failure(
                    StateError{"string[]", "mixed", ErrorContext{formatError(key, "contains non-string element")}});
            }
        }
        return R::success(std::move(result));
    }
    return R::failure(StateError{"array", "other", ErrorContext{formatError(key, "is not an array")}});
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

using RegistryResult = Result<const DataLoader*, NotFound, IOError>;

RegistryResult DataRegistry::get(const std::filesystem::path& path) {
    auto canonical = std::filesystem::absolute(path).string();

    std::lock_guard lock(mutex_);
    auto it = cache_.find(canonical);
    if (it != cache_.end()) {
        return RegistryResult::success(&it->second);
    }

    auto result = DataLoader::load(path);
    if (result.isFailure()) {
        return result.match([](const DataLoader&) -> RegistryResult { __builtin_unreachable(); },
                            [](auto err) -> RegistryResult {
                                return std::visit(
                                    [](auto& e) -> RegistryResult { return RegistryResult::failure(std::move(e)); },
                                    err.variant());
                            });
    }

    auto [inserted, _] = cache_.emplace(canonical, std::move(result.value()));
    return RegistryResult::success(&inserted->second);
}

RegistryResult DataRegistry::reload(const std::filesystem::path& path) {
    auto canonical = std::filesystem::absolute(path).string();

    auto result = DataLoader::load(path);
    if (result.isFailure()) {
        return result.match([](const DataLoader&) -> RegistryResult { __builtin_unreachable(); },
                            [](auto err) -> RegistryResult {
                                return std::visit(
                                    [](auto& e) -> RegistryResult { return RegistryResult::failure(std::move(e)); },
                                    err.variant());
                            });
    }

    std::lock_guard lock(mutex_);
    cache_.erase(canonical);
    auto [it, _] = cache_.emplace(canonical, std::move(result.value()));
    return RegistryResult::success(&it->second);
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

} // namespace recurse
