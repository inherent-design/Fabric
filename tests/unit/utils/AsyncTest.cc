#include <atomic>
#include <chrono>
#include <thread>

#include "fabric/core/Async.hh"
#include <gtest/gtest.h>

namespace fabric {
namespace Tests {

// Async uses a static global io_context, so tests share state. init() and
// shutdown() are not safe to call multiple times in a single process (the
// backend thread may not restart cleanly). These tests exercise what we
// can safely test within that constraint.

class AsyncTest : public ::testing::Test {
  protected:
    void SetUp() override {
        fabric::async::init();
        // After a previous shutdown(), the io_context is stopped. poll()
        // restarts it so handlers posted in the test are actually processed.
        fabric::async::poll();
    }
    void TearDown() override { fabric::async::shutdown(); }
};

TEST_F(AsyncTest, PollProcessesPostedWork) {
    std::atomic<bool> executed{false};

    asio::post(fabric::async::context(), [&executed]() { executed = true; });

    fabric::async::poll();
    EXPECT_TRUE(executed);
}

TEST_F(AsyncTest, MakeTimerCreatesValidTimer) {
    auto timer = fabric::async::makeTimer();
    // Timer should be bound to the async context
    timer.expires_after(std::chrono::milliseconds(1));

    std::atomic<bool> fired{false};
    timer.async_wait([&fired](const asio::error_code& ec) {
        if (!ec)
            fired = true;
    });

    // Process the timer
    fabric::async::poll();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    fabric::async::poll();

    EXPECT_TRUE(fired);
}

TEST_F(AsyncTest, MakeTimerWithDuration) {
    auto timer = fabric::async::makeTimer(std::chrono::milliseconds(1));

    std::atomic<bool> fired{false};
    timer.async_wait([&fired](const asio::error_code& ec) {
        if (!ec)
            fired = true;
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    fabric::async::poll();

    EXPECT_TRUE(fired);
}

TEST_F(AsyncTest, MakeStrandReturnsValidExecutor) {
    auto strand = fabric::async::makeStrand();

    std::atomic<int> counter{0};
    asio::post(strand, [&counter]() { counter++; });
    asio::post(strand, [&counter]() { counter++; });

    fabric::async::poll();
    EXPECT_EQ(counter.load(), 2);
}

TEST_F(AsyncTest, ContextReturnsReference) {
    auto& ctx = fabric::async::context();
    // Should be able to post work directly
    std::atomic<bool> ran{false};
    asio::post(ctx, [&ran]() { ran = true; });
    fabric::async::poll();
    EXPECT_TRUE(ran);
}

} // namespace Tests
} // namespace fabric
