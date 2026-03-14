#pragma once

#include <exception>
#include <string>

namespace fabric {

/// Custom exception base for Fabric Engine errors.
class FabricException : public std::exception {
  public:
    explicit FabricException(const std::string& message);
    const char* what() const noexcept override;

  private:
    std::string message_;
};

[[noreturn]] void throwError(const std::string& message);

} // namespace fabric
