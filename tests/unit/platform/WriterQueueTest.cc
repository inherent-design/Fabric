#include "fabric/platform/WriterQueue.hh"
#include <algorithm>
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

TEST(WriterQueueTest, MultiProducerSharedVectorNoMutex) {
    // The critical invariant: if WriterQueue is truly serial, writing to a
    // shared vector WITHOUT a mutex is safe because only one task executes
    // at a time. A data race here would prove broken serialization.
    WriterQueue q;
    std::vector<int> sequence; // deliberately unprotected

    constexpr int K_NUM_PRODUCERS = 4;
    constexpr int K_TASKS_PER_PRODUCER = 100;
    constexpr int K_TOTAL = K_NUM_PRODUCERS * K_TASKS_PER_PRODUCER;

    std::latch startLatch(K_NUM_PRODUCERS);
    std::vector<std::thread> producers;
    producers.reserve(K_NUM_PRODUCERS);

    for (int p = 0; p < K_NUM_PRODUCERS; ++p) {
        producers.emplace_back([&q, &sequence, &startLatch, p]() {
            startLatch.arrive_and_wait();
            for (int i = 0; i < K_TASKS_PER_PRODUCER; ++i) {
                int tag = p * K_TASKS_PER_PRODUCER + i;
                q.submit([&sequence, tag]() { sequence.push_back(tag); });
            }
        });
    }

    for (auto& t : producers) {
        t.join();
    }
    q.drain();

    ASSERT_EQ(sequence.size(), static_cast<size_t>(K_TOTAL)) << "Every submitted task must execute exactly once";

    // Verify no duplicate sequence numbers
    std::vector<int> sorted = sequence;
    std::sort(sorted.begin(), sorted.end());
    for (size_t i = 1; i < sorted.size(); ++i) {
        EXPECT_NE(sorted[i], sorted[i - 1]) << "Duplicate tag: " << sorted[i];
    }

    // Verify per-producer FIFO: within each producer's submissions,
    // relative ordering must be preserved
    std::vector<std::vector<int>> perProducer(K_NUM_PRODUCERS);
    for (int tag : sequence) {
        int p = tag / K_TASKS_PER_PRODUCER;
        perProducer[static_cast<size_t>(p)].push_back(tag);
    }
    for (int p = 0; p < K_NUM_PRODUCERS; ++p) {
        const auto& tags = perProducer[static_cast<size_t>(p)];
        ASSERT_EQ(tags.size(), static_cast<size_t>(K_TASKS_PER_PRODUCER))
            << "Producer " << p << " should have exactly " << K_TASKS_PER_PRODUCER << " entries";
        for (size_t i = 1; i < tags.size(); ++i) {
            EXPECT_LT(tags[i - 1], tags[i]) << "Producer " << p << " FIFO violated at index " << i;
        }
    }
}

TEST(WriterQueueTest, ConcurrentSubmitAndDrainNoDeadlock) {
    // Verify that concurrent submit + drain does not deadlock or lose tasks.
    // Multiple threads submit while other threads drain; all tasks must execute.
    WriterQueue q;
    std::atomic<int> executed{0};

    constexpr int K_SUBMITTERS = 4;
    constexpr int K_DRAINERS = 2;
    constexpr int K_TASKS_PER_SUBMITTER = 50;
    constexpr int K_DRAINS_PER_DRAINER = 10;

    std::latch startLatch(K_SUBMITTERS + K_DRAINERS);
    std::vector<std::thread> threads;
    threads.reserve(K_SUBMITTERS + K_DRAINERS);

    for (int s = 0; s < K_SUBMITTERS; ++s) {
        threads.emplace_back([&q, &executed, &startLatch]() {
            startLatch.arrive_and_wait();
            for (int i = 0; i < K_TASKS_PER_SUBMITTER; ++i) {
                q.submit([&executed]() { executed.fetch_add(1, std::memory_order_relaxed); });
            }
        });
    }

    for (int d = 0; d < K_DRAINERS; ++d) {
        threads.emplace_back([&q, &startLatch]() {
            startLatch.arrive_and_wait();
            for (int i = 0; i < K_DRAINS_PER_DRAINER; ++i) {
                q.drain();
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // Final drain to ensure all remaining tasks complete
    q.drain();

    EXPECT_EQ(executed.load(), K_SUBMITTERS * K_TASKS_PER_SUBMITTER)
        << "All submitted tasks must execute; none lost to concurrent drain";
}

TEST(WriterQueueTest, MultiProducerHighContention) {
    // Stress test: many threads, many short tasks, verifying completeness
    // under high lock contention on the submit path
    WriterQueue q;
    std::atomic<int> count{0};

    constexpr int K_NUM_THREADS = 16;
    constexpr int K_TASKS_PER_THREAD = 200;

    std::latch startLatch(K_NUM_THREADS);
    std::vector<std::thread> threads;
    threads.reserve(K_NUM_THREADS);

    for (int t = 0; t < K_NUM_THREADS; ++t) {
        threads.emplace_back([&q, &count, &startLatch]() {
            startLatch.arrive_and_wait();
            for (int i = 0; i < K_TASKS_PER_THREAD; ++i) {
                q.submit([&count]() { count.fetch_add(1, std::memory_order_relaxed); });
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }
    q.drain();

    EXPECT_EQ(count.load(), K_NUM_THREADS * K_TASKS_PER_THREAD) << "All tasks must complete under high contention";
}
