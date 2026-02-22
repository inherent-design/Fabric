#pragma once

#include <cstdint>
#include <exception>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace fabric {

/**
 * @brief Custom exception class for Fabric Engine errors
 */
class FabricException : public std::exception {
public:
  explicit FabricException(const std::string &message);
  const char *what() const noexcept override;

private:
  std::string message;
};

[[noreturn]] void throwError(const std::string &message);

// Lightweight error code for hot paths where exceptions are too expensive
enum class ErrorCode : uint16_t {
  Ok = 0,
  BufferOverrun,
  InvalidState,
  Timeout,
  ConnectionReset,
  PermissionDenied,
  NotFound,
  AlreadyExists,
  ResourceExhausted,
  Internal
};

std::string_view errorCodeToString(ErrorCode code);

// Result type combining value + error. Move-only.
template <typename T>
class Result {
public:
  static Result ok(T value) { return Result(std::move(value)); }

  static Result error(ErrorCode code, std::string message = "") {
    return Result(code, std::move(message));
  }

  bool isOk() const { return code_ == ErrorCode::Ok; }
  bool isError() const { return code_ != ErrorCode::Ok; }
  ErrorCode code() const { return code_; }
  const std::string &message() const { return message_; }

  T &value() {
    if (isError())
      throwError("Result contains error: " + message_);
    return *value_;
  }

  const T &value() const {
    if (isError())
      throwError("Result contains error: " + message_);
    return *value_;
  }

  T valueOr(T defaultValue) const {
    if (isOk())
      return *value_;
    return defaultValue;
  }

  Result(Result &&) = default;
  Result &operator=(Result &&) = default;
  Result(const Result &) = delete;
  Result &operator=(const Result &) = delete;

private:
  explicit Result(T value)
      : code_(ErrorCode::Ok), value_(std::move(value)) {}

  Result(ErrorCode code, std::string message)
      : code_(code), message_(std::move(message)) {}

  ErrorCode code_;
  std::string message_;
  std::optional<T> value_;
};

template <>
class Result<void> {
public:
  static Result ok() { return Result(); }

  static Result error(ErrorCode code, std::string message = "") {
    return Result(code, std::move(message));
  }

  bool isOk() const { return code_ == ErrorCode::Ok; }
  bool isError() const { return code_ != ErrorCode::Ok; }
  ErrorCode code() const { return code_; }
  const std::string &message() const { return message_; }

  Result(Result &&) = default;
  Result &operator=(Result &&) = default;
  Result(const Result &) = delete;
  Result &operator=(const Result &) = delete;

private:
  Result() : code_(ErrorCode::Ok) {}

  Result(ErrorCode code, std::string message)
      : code_(code), message_(std::move(message)) {}

  ErrorCode code_;
  std::string message_;
};

} // namespace fabric
