#include "fabric/utils/ErrorHandling.hh"
#include "fabric/log/Log.hh"

namespace fabric {

FabricException::FabricException(const std::string& message) : message_(message) {}

const char* FabricException::what() const noexcept {
    return message_.c_str();
}

void throwError(const std::string& message) {
    FABRIC_LOG_ERROR("FabricException: {}", message);
    throw FabricException(message);
}

} // namespace fabric
