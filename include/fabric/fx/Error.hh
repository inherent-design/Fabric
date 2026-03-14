#pragma once

#include <concepts>
#include <source_location>
#include <string>
#include <string_view>

namespace fabric::fx {

/// A type is a TaggedError if it exposes a static K_TAG string_view.
template <typename E>
concept TaggedError = requires {
    { E::K_TAG } -> std::convertible_to<std::string_view>;
};

/// Common context carried by all domain errors.
struct ErrorContext {
    std::string message;
    std::source_location location;

    ErrorContext(std::string msg, std::source_location loc = std::source_location::current())
        : message(std::move(msg)), location(loc) {}
};

/// I/O failure (file, database, network).
struct IOError {
    static constexpr std::string_view K_TAG = "io";
    std::string path;
    int code{0};
    ErrorContext ctx;
};

/// Invalid state transition or precondition violation.
struct StateError {
    static constexpr std::string_view K_TAG = "state";
    std::string expected;
    std::string actual;
    ErrorContext ctx;
};

/// Resource lookup miss.
struct NotFound {
    static constexpr std::string_view K_TAG = "not_found";
    std::string key;
    ErrorContext ctx;
};

/// Thread safety or synchronization violation.
struct ConcurrencyError {
    static constexpr std::string_view K_TAG = "concurrency";
    ErrorContext ctx;
};

static_assert(TaggedError<IOError>);
static_assert(TaggedError<StateError>);
static_assert(TaggedError<NotFound>);
static_assert(TaggedError<ConcurrencyError>);

} // namespace fabric::fx
