#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

namespace fabric {

// Binary data alias for codec/network/storage domains
using BinaryData = std::vector<uint8_t>;

using Variant = std::variant<std::nullptr_t, bool, int, float, double, std::string, std::vector<uint8_t>>;

template <typename T> using StringMap = std::map<std::string, T>;

template <typename T> using Optional = std::optional<T>;

// Forward declarations for use in type aliases
struct Token;
enum class TokenType;

/**
 * @brief Type for a map of string to Token pairs
 */
using TokenMap = StringMap<Token>;

/**
 * @brief Type for an optional Token
 */
using OptionalToken = Optional<Token>;

/**
 * @brief Type for a pair containing a token type and a boolean flag
 */
using TokenTypeOptionPair = std::pair<TokenType, bool>;

/**
 * @brief Type for a map of string to token type option pairs
 */
using TokenTypeOptionsMap = StringMap<TokenTypeOptionPair>;

/**
 * @brief Type for a map of string to string
 */
using StringStringMap = StringMap<std::string>;

} // namespace fabric
