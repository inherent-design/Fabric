#pragma once

#include <string>

namespace fabric {
namespace utils {

// Thread-safe. Generates prefix + `length` random hex digits.
std::string generateUniqueId(const std::string& prefix, int length = 8);

} // namespace utils
} // namespace fabric
