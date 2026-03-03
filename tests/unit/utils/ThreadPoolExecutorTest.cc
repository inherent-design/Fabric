#include "fabric/utils/ThreadPoolExecutor.hh"
#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <numeric>
#include <thread>
#include <vector>

namespace fabric {
namespace Tests {

using Utils::ThreadPoolExecutor;
using Utils::ThreadPoolTimeoutException;

// -- Construction --

TEST(ThreadPoolExecutorTest, ConstructsWithRequestedThreadCount) {
    ThreadPoolExecutor pool(4);
    EXPECT_EQ(pool.getThreadCount(), 4u);
    EXPECT_FALSE(pool.isShutdown());
}

TEST(ThreadPoolExecutorTest, ConstructsWithZeroDefaultsToHardwareConcurrency) {
    ThreadPoolExecutor pool(0);
    EXPECT_GE(pool.getThreadCount(), 1u);
}

// -- Submit --

TEST(ThreadPoolExecutorTest, SubmitReturnsCorrectResult) {
    ThreadPoolExecutor pool(2);
    auto future = pool.submit([]() { return 42; });
    EXPECT_EQ(future.get(), 42);
}

TEST(ThreadPoolExecutorTest, SubmitVoidTask) {
    ThreadPoolExecutor pool(2);
    std::atomic<bool> executed{false};
    auto future = pool.submit([&executed]() { executed = true; });
    future.get();
    EXPECT_TRUE(executed);
}

TEST(ThreadPoolExecutorTest, SubmitMultipleTasks) {
    ThreadPoolExecutor pool(4);
    constexpr int kTaskCount = 100;
    std::vector<std::future<int>> futures;
    futures.reserve(kTaskCount);

    for (int i = 0; i < kTaskCount; ++i) {
        futures.push_back(pool.submit([i]() { return i * 2; }));
    }

    for (int i = 0; i < kTaskCount; ++i) {
        EXPECT_EQ(futures[i].get(), i * 2);
    }
}

TEST(ThreadPoolExecutorTest, SubmitPropagatesException) {
    ThreadPoolExecutor pool(2);
    auto future = pool.submit([]() -> int { throw std::runtime_error("test error"); });
    EXPECT_THROW(future.get(), std::runtime_error);
}

TEST(ThreadPoolExecutorTest, SubmitAfterShutdownThrows) {
    ThreadPoolExecutor pool(2);
    pool.shutdown();
    EXPECT_THROW(pool.submit([]() {}), FabricException);
}

// -- submitWithTimeout --

TEST(ThreadPoolExecutorTest, SubmitWithTimeoutCompletes) {
    ThreadPoolExecutor pool(2);
    auto future = pool.submitWithTimeout(std::chrono::seconds(5), []() { return 99; });
    EXPECT_EQ(future.get(), 99);
}

TEST(ThreadPoolExecutorTest, SubmitWithTimeoutExpires) {
    ThreadPoolExecutor pool(1);

    // Block the only worker thread
    std::promise<void> blocker;
    auto blockFuture = blocker.get_future();
    pool.submit([&blockFuture]() { blockFuture.wait(); });

    // Submit a task with a very short deadline
    auto result = pool.submitWithTimeout(std::chrono::milliseconds(10), []() { return 42; });

    // Wait past the deadline, then unblock
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    blocker.set_value();

    EXPECT_THROW(result.get(), ThreadPoolTimeoutException);

    // Shut down before blockFuture goes out of scope
    pool.shutdown();
}

// -- Shutdown --

TEST(ThreadPoolExecutorTest, ShutdownJoinsAllThreads) {
    ThreadPoolExecutor pool(4);
    std::atomic<int> counter{0};
    std::vector<std::future<void>> futures;
    futures.reserve(20);

    for (int i = 0; i < 20; ++i) {
        futures.push_back(pool.submit([&counter]() { counter++; }));
    }

    // Wait for all tasks to complete before shutdown
    for (auto& f : futures) {
        f.get();
    }

    EXPECT_TRUE(pool.shutdown(std::chrono::seconds(5)));
    EXPECT_TRUE(pool.isShutdown());
    EXPECT_EQ(counter.load(), 20);
}

TEST(ThreadPoolExecutorTest, ShutdownIdempotent) {
    ThreadPoolExecutor pool(2);
    EXPECT_TRUE(pool.shutdown());
    EXPECT_TRUE(pool.shutdown());
}

// -- pauseForTesting --

TEST(ThreadPoolExecutorTest, PausedRunsTasksInline) {
    ThreadPoolExecutor pool(2);
    pool.pauseForTesting();
    EXPECT_TRUE(pool.isPausedForTesting());

    // Tasks should run synchronously in the calling thread
    auto callerThread = std::this_thread::get_id();
    std::thread::id taskThread;

    auto future = pool.submit([&taskThread]() {
        taskThread = std::this_thread::get_id();
        return 7;
    });

    EXPECT_EQ(future.get(), 7);
    EXPECT_EQ(taskThread, callerThread);
}

TEST(ThreadPoolExecutorTest, ResumeAfterTestingRestoresThreads) {
    ThreadPoolExecutor pool(2);
    pool.pauseForTesting();
    pool.resumeAfterTesting();
    EXPECT_FALSE(pool.isPausedForTesting());

    // Should still execute tasks normally
    auto future = pool.submit([]() { return 42; });
    EXPECT_EQ(future.get(), 42);
}

// -- setThreadCount --

TEST(ThreadPoolExecutorTest, SetThreadCountIncrease) {
    ThreadPoolExecutor pool(2);
    pool.setThreadCount(4);
    EXPECT_EQ(pool.getThreadCount(), 4u);

    // Verify the pool still works
    auto future = pool.submit([]() { return 1; });
    EXPECT_EQ(future.get(), 1);
}

TEST(ThreadPoolExecutorTest, SetThreadCountDecrease) {
    ThreadPoolExecutor pool(4);
    pool.setThreadCount(2);
    EXPECT_EQ(pool.getThreadCount(), 2u);

    // Verify the pool still works
    auto future = pool.submit([]() { return 1; });
    EXPECT_EQ(future.get(), 1);
}

TEST(ThreadPoolExecutorTest, SetThreadCountZeroThrows) {
    ThreadPoolExecutor pool(2);
    EXPECT_THROW(pool.setThreadCount(0), FabricException);
}

// -- Queue --

TEST(ThreadPoolExecutorTest, QueuedTaskCountReflectsState) {
    ThreadPoolExecutor pool(1);

    // Block the worker
    std::promise<void> blocker;
    auto blockFuture = blocker.get_future();
    pool.submit([&blockFuture]() { blockFuture.wait(); });

    // Give the worker time to pick up the blocking task
    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    // Queue additional tasks
    pool.submit([]() {});
    pool.submit([]() {});

    EXPECT_EQ(pool.getQueuedTaskCount(), 2u);

    // Unblock and shut down before blockFuture goes out of scope,
    // since the blocking task captures it by reference
    blocker.set_value();
    pool.shutdown();
}

// -- Concurrent correctness --

TEST(ThreadPoolExecutorTest, ConcurrentSubmitsProduceCorrectResults) {
    ThreadPoolExecutor pool(4);
    constexpr int kTasks = 200;
    std::atomic<int> sum{0};
    std::vector<std::future<void>> futures;
    futures.reserve(kTasks);

    for (int i = 0; i < kTasks; ++i) {
        futures.push_back(pool.submit([&sum, i]() { sum += i; }));
    }

    for (auto& f : futures) {
        f.get();
    }

    int expected = kTasks * (kTasks - 1) / 2;
    EXPECT_EQ(sum.load(), expected);
}

} // namespace Tests
} // namespace fabric
