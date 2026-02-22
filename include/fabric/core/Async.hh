#pragma once

#include <asio/io_context.hpp>
#include <asio/executor_work_guard.hpp>
#include <asio/co_spawn.hpp>
#include <asio/detached.hpp>
#include <asio/awaitable.hpp>
#include <asio/use_awaitable.hpp>
#include <asio/steady_timer.hpp>
#include <asio/strand.hpp>
#include <asio/as_tuple.hpp>

namespace fabric::async {

/// Get the engine's main io_context. Call poll() each frame.
asio::io_context& context();

/// Initialize the async subsystem. Call once at startup.
void init();

/// Shutdown: release work guard, drain remaining handlers.
void shutdown();

/// Non-blocking poll: process all ready handlers. Call once per frame.
/// Equivalent to io_context::poll() + restart().
void poll();

/// Blocking run for server-mode applications.
/// Calls io_context::run(). Blocks until shutdown() or all work completes.
void run();

/// Strand for per-connection/per-session serialization.
/// Returns an executor that serializes handlers (no concurrent execution).
asio::strand<asio::io_context::executor_type> makeStrand();

/// Create a steady_timer bound to the async context.
asio::steady_timer makeTimer();
asio::steady_timer makeTimer(std::chrono::steady_clock::duration duration);

/// Default completion token: returns tuple<error_code, T> instead of throwing.
inline const auto use_nothrow = asio::as_tuple(asio::use_awaitable);

} // namespace fabric::async
