#pragma once

#include <string>

namespace fabric {

class Utils {
public:
  // Thread-safe. Generates prefix + `length` random hex digits.
  static std::string generateUniqueId(const std::string& prefix, int length = 8);
};

} // namespace fabric
