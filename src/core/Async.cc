#include "fabric/core/Async.hh"
#include "fabric/core/Log.hh"

#include <optional>

namespace fabric::async {

static asio::io_context io_ctx;
static std::optional<asio::executor_work_guard<asio::io_context::executor_type>> work_guard;

asio::io_context& context() {
  return io_ctx;
}

void init() {
  work_guard.emplace(asio::make_work_guard(io_ctx));
  FABRIC_LOG_INFO("Async: subsystem initialized");
}

void shutdown() {
  FABRIC_LOG_INFO("Async: subsystem shutting down");
  work_guard.reset();
  io_ctx.run();
}

void poll() {
  io_ctx.poll();
  io_ctx.restart();
}

void run() {
  io_ctx.run();
  io_ctx.restart();
}

asio::strand<asio::io_context::executor_type> makeStrand() {
  return asio::make_strand(io_ctx);
}

asio::steady_timer makeTimer() {
  return asio::steady_timer(io_ctx);
}

asio::steady_timer makeTimer(std::chrono::steady_clock::duration duration) {
  return asio::steady_timer(io_ctx, duration);
}

} // namespace fabric::async
