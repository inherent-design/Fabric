#include "fabric/utils/ErrorHandling.hh"
#include "fabric/core/Log.hh"

namespace fabric {

FabricException::FabricException(const std::string &message)
    : message(message) {}

const char *FabricException::what() const noexcept { return message.c_str(); }

void throwError(const std::string &message) {
  FABRIC_LOG_ERROR("FabricException: {}", message);
  throw FabricException(message);
}

std::string_view errorCodeToString(ErrorCode code) {
  switch (code) {
  case ErrorCode::Ok:
    return "Ok";
  case ErrorCode::BufferOverrun:
    return "BufferOverrun";
  case ErrorCode::InvalidState:
    return "InvalidState";
  case ErrorCode::Timeout:
    return "Timeout";
  case ErrorCode::ConnectionReset:
    return "ConnectionReset";
  case ErrorCode::PermissionDenied:
    return "PermissionDenied";
  case ErrorCode::NotFound:
    return "NotFound";
  case ErrorCode::AlreadyExists:
    return "AlreadyExists";
  case ErrorCode::ResourceExhausted:
    return "ResourceExhausted";
  case ErrorCode::Internal:
    return "Internal";
  }
  return "Unknown";
}

} // namespace fabric
