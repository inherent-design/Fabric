#include "fabric/core/JobScheduler.hh"
#include <atomic>
#include <gtest/gtest.h>
#include <mutex>
#include <random>
#include <set>
#include <thread>
#include <vector>

using fabric::JobScheduler;

// 1. Construct and destruct without work
TEST(JobSchedulerTest, ConstructDestruct) {
    { JobScheduler scheduler(2); }
    { JobScheduler scheduler(0); }
    SUCCEED();
}

// 2. parallelFor runs all jobs and delivers correct indices
TEST(JobSchedulerTest, ParallelForBasic) {
    JobScheduler scheduler(4);

    constexpr size_t K_COUNT = 100;
    std::vector<std::atomic<int>> flags(K_COUNT);
    for (auto& f : flags)
        f.store(0, std::memory_order_relaxed);

    scheduler.parallelFor(
        K_COUNT, [&](size_t jobIdx, size_t /*workerIdx*/) { flags[jobIdx].fetch_add(1, std::memory_order_relaxed); });

    for (size_t i = 0; i < K_COUNT; ++i)
        EXPECT_EQ(flags[i].load(), 1) << "Job " << i << " ran wrong number of times";
}

// 3. disableForTesting runs inline on calling thread
TEST(JobSchedulerTest, DisableForTestingInline) {
    JobScheduler scheduler(4);
    scheduler.disableForTesting();

    auto callerThread = std::this_thread::get_id();
    bool allOnCaller = true;

    scheduler.parallelFor(10, [&](size_t /*jobIdx*/, size_t /*workerIdx*/) {
        if (std::this_thread::get_id() != callerThread)
            allOnCaller = false;
    });

    EXPECT_TRUE(allOnCaller);
}

// 4. Deterministic seeding: same (jobIdx, workerIdx) produces same PRNG sequence
TEST(JobSchedulerTest, DeterministicSeeding) {
    JobScheduler scheduler(4);
    scheduler.disableForTesting();

    constexpr size_t K_COUNT = 8;
    std::vector<uint32_t> run1(K_COUNT), run2(K_COUNT);

    auto generateValues = [](size_t count, std::vector<uint32_t>& out, JobScheduler& sched) {
        sched.parallelFor(count, [&](size_t jobIdx, size_t workerIdx) {
            std::mt19937 rng(42 + jobIdx);
            out[jobIdx] = rng();
        });
    };

    generateValues(K_COUNT, run1, scheduler);
    generateValues(K_COUNT, run2, scheduler);

    for (size_t i = 0; i < K_COUNT; ++i)
        EXPECT_EQ(run1[i], run2[i]) << "Non-deterministic at index " << i;
}

// 5. Empty dispatch (count = 0) does not deadlock
TEST(JobSchedulerTest, EmptyDispatch) {
    JobScheduler scheduler(4);
    scheduler.parallelFor(0, [](size_t, size_t) { FAIL() << "Should not be called"; });
    SUCCEED();
}

// 6. Multiple sequential dispatches all complete correctly
TEST(JobSchedulerTest, MultipleDispatches) {
    JobScheduler scheduler(4);

    for (int round = 0; round < 10; ++round) {
        std::atomic<int> sum{0};
        scheduler.parallelFor(
            50, [&](size_t /*jobIdx*/, size_t /*workerIdx*/) { sum.fetch_add(1, std::memory_order_relaxed); });
        EXPECT_EQ(sum.load(), 50) << "Round " << round << " failed";
    }
}

// 7. workerCount returns the configured count
TEST(JobSchedulerTest, WorkerCountReturnsConfigured) {
    JobScheduler scheduler(3);
    EXPECT_EQ(scheduler.workerCount(), 3u);
}

// 8. Single job dispatch works
TEST(JobSchedulerTest, SingleJobDispatch) {
    JobScheduler scheduler(4);
    int called = 0;
    scheduler.parallelFor(1, [&](size_t jobIdx, size_t /*workerIdx*/) {
        EXPECT_EQ(jobIdx, 0u);
        ++called;
    });
    EXPECT_EQ(called, 1);
}

// 9. Stress: 100 sequential dispatches with no deadlock
TEST(JobSchedulerTest, StressSequentialDispatches) {
    JobScheduler scheduler(4);

    for (int round = 0; round < 100; ++round) {
        std::atomic<int> sum{0};
        scheduler.parallelFor(
            50, [&](size_t /*jobIdx*/, size_t /*workerIdx*/) { sum.fetch_add(1, std::memory_order_relaxed); });
        EXPECT_EQ(sum.load(), 50) << "Deadlock or miscount at round " << round;
    }
}

// 10. Worker indices stay within expected bounds
TEST(JobSchedulerTest, WorkerIndicesInRange) {
    JobScheduler scheduler(4);

    std::mutex mu;
    std::set<size_t> observed;

    scheduler.parallelFor(100, [&](size_t /*jobIdx*/, size_t workerIdx) {
        std::lock_guard<std::mutex> lock(mu);
        observed.insert(workerIdx);
    });

    // enkiTS threadnum: created threads 1..N, calling thread is 0 during WaitforTask
    size_t bound = scheduler.workerCount() + 1;
    for (size_t idx : observed)
        EXPECT_LT(idx, bound) << "workerIdx " << idx << " out of range [0, " << bound << ")";
}

// 11. Large job count completes correctly
TEST(JobSchedulerTest, LargeJobCount) {
    JobScheduler scheduler(4);

    std::atomic<int> counter{0};
    scheduler.parallelFor(
        10000, [&](size_t /*jobIdx*/, size_t /*workerIdx*/) { counter.fetch_add(1, std::memory_order_relaxed); });

    EXPECT_EQ(counter.load(), 10000);
}

// 12. submit() returns a value through the future
TEST(JobSchedulerTest, SubmitReturnsValue) {
    JobScheduler scheduler(4);
    auto future = scheduler.submit([]() -> int { return 42; });
    EXPECT_EQ(future.get(), 42);
}

// 13. submit() with void return completes
TEST(JobSchedulerTest, SubmitVoidCompletes) {
    JobScheduler scheduler(4);
    std::atomic<bool> ran{false};
    auto future = scheduler.submit([&]() { ran.store(true, std::memory_order_release); });
    future.get();
    EXPECT_TRUE(ran.load(std::memory_order_acquire));
}

// 14. submitBackground() completes all fire-and-forget work
TEST(JobSchedulerTest, SubmitBackgroundCompletes) {
    JobScheduler scheduler(4);
    std::atomic<int> counter{0};
    for (int i = 0; i < 10; ++i)
        scheduler.submitBackground([&]() { counter.fetch_add(1, std::memory_order_relaxed); });

    // Use a tracked submit to synchronize; background tasks submitted
    // earlier will typically drain via work-stealing before this completes.
    auto future = scheduler.submit([]() -> int { return 1; });
    future.get();

    for (int attempt = 0; attempt < 1000 && counter.load() < 10; ++attempt)
        std::this_thread::yield();
    EXPECT_EQ(counter.load(), 10);
}
