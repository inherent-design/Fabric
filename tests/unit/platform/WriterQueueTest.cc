#include "fabric/platform/WriterQueue.hh"
#include <atomic>
#include <chrono>
#include <gtest/gtest.h>
#include <latch>
#include <thread>
#include <vector>

using namespace fabric::platform;

TEST(WriterQueueTest, SerialExecutionOrder) {
    WriterQueue q;
    std::vector<int> order;
    std::mutex orderMutex;

    for (int i = 0; i < 10; ++i) {
        q.submit([i, &order, &orderMutex]() {
            std::lock_guard lock(orderMutex);
            order.push_back(i);
        });
    }

    q.drain();
    ASSERT_EQ(order.size(), 10u);
    for (int i = 0; i < 10; ++i) {
        EXPECT_EQ(order[static_cast<size_t>(i)], i);
    }
}

TEST(WriterQueueTest, DrainBlocksUntilEmpty) {
    WriterQueue q;
    std::atomic<int> count{0};

    for (int i = 0; i < 100; ++i) {
        q.submit([&count]() { count.fetch_add(1, std::memory_order_relaxed); });
    }

    q.drain();
    EXPECT_EQ(count.load(), 100);
}

TEST(WriterQueueTest, SubmitAfterDrainWorks) {
    WriterQueue q;
    std::atomic<int> count{0};

    q.submit([&count]() { count.fetch_add(1, std::memory_order_relaxed); });
    q.drain();
    EXPECT_EQ(count.load(), 1);

    q.submit([&count]() { count.fetch_add(1, std::memory_order_relaxed); });
    q.drain();
    EXPECT_EQ(count.load(), 2);
}

TEST(WriterQueueTest, DestructorDrains) {
    std::atomic<int> count{0};

    {
        WriterQueue q;
        for (int i = 0; i < 50; ++i) {
            q.submit([&count]() { count.fetch_add(1, std::memory_order_relaxed); });
        }
    } // destructor calls shutdown(), which drains remaining work

    EXPECT_EQ(count.load(), 50);
}

TEST(WriterQueueTest, ConcurrentSubmitsSerialize) {
    WriterQueue q;
    std::atomic<int> maxConcurrent{0};
    std::atomic<int> current{0};
    std::atomic<int> completed{0};

    constexpr int K_NUM_THREADS = 8;
    constexpr int K_TASKS_PER_THREAD = 20;
    std::latch startLatch(K_NUM_THREADS);

    std::vector<std::thread> threads;
    threads.reserve(K_NUM_THREADS);

    for (int t = 0; t < K_NUM_THREADS; ++t) {
        threads.emplace_back([&q, &maxConcurrent, &current, &completed, &startLatch]() {
            startLatch.arrive_and_wait();
            for (int i = 0; i < K_TASKS_PER_THREAD; ++i) {
                q.submit([&maxConcurrent, &current, &completed]() {
                    int c = current.fetch_add(1, std::memory_order_seq_cst) + 1;
                    int prev = maxConcurrent.load(std::memory_order_relaxed);
                    while (c > prev && !maxConcurrent.compare_exchange_weak(prev, c, std::memory_order_relaxed)) {}
                    current.fetch_sub(1, std::memory_order_seq_cst);
                    completed.fetch_add(1, std::memory_order_relaxed);
                });
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }
    q.drain();

    EXPECT_EQ(completed.load(), K_NUM_THREADS * K_TASKS_PER_THREAD);
    EXPECT_EQ(maxConcurrent.load(), 1) << "Tasks must execute serially, not concurrently";
}

TEST(WriterQueueTest, ShutdownPreventsNewSubmissions) {
    WriterQueue q;
    std::atomic<int> count{0};

    q.submit([&count]() { count.fetch_add(1, std::memory_order_relaxed); });
    q.drain();
    EXPECT_EQ(count.load(), 1);

    q.shutdown();

    // Submit after shutdown: task should be discarded
    q.submit([&count]() { count.fetch_add(1, std::memory_order_relaxed); });
    EXPECT_EQ(count.load(), 1) << "Task submitted after shutdown must not execute";
}

TEST(WriterQueueTest, ShutdownIdempotent) {
    WriterQueue q;
    q.shutdown();
    q.shutdown(); // no crash
}

TEST(WriterQueueTest, TaskExceptionDoesNotStopQueue) {
    WriterQueue q;
    std::atomic<int> count{0};

    q.submit([]() { throw std::runtime_error("deliberate test exception"); });
    q.submit([&count]() { count.fetch_add(1, std::memory_order_relaxed); });
    q.drain();

    EXPECT_EQ(count.load(), 1) << "Second task must run despite first task throwing";
}
