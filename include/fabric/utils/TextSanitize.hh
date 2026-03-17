#pragma once

#include <string>
#include <string_view>

namespace fabric {
namespace utils {

/// Escape a string for safe embedding in RML (XML-subset) markup.
inline std::string rmlEscape(std::string_view input) {
    std::string result;
    result.reserve(input.size());
    for (char c : input) {
        switch (c) {
            case '&':
                result += "&amp;";
                break;
            case '<':
                result += "&lt;";
                break;
            case '>':
                result += "&gt;";
                break;
            case '"':
                result += "&quot;";
                break;
            case '\'':
                result += "&#39;";
                break;
            default:
                result += c;
                break;
        }
    }
    return result;
}

/// Escape a string for safe embedding in SQL string literals.
inline std::string sqlEscape(std::string_view input) {
    std::string result;
    result.reserve(input.size());
    for (char c : input) {
        switch (c) {
            case '\'':
                result += "''";
                break;
            case '\\':
                result += "\\\\";
                break;
            default:
                result += c;
                break;
        }
    }
    return result;
}

} // namespace utils
} // namespace fabric
