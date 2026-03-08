#pragma once
#include <cstddef>
#include <cstdint>
#include <functional>
#include <random>
#include <vector>

namespace recurse::simulation {

/// Thread pool for parallel chunk simulation dispatch.
/// Each task receives a per-worker std::mt19937 seeded deterministically
/// from baseSeed + workerIndex, ensuring no shared mutable PRNG state.
class SimWorkerPool {
  public:
    /// @param threadCount Number of worker threads. 0 = hardware_concurrency - 2 (min 1).
    explicit SimWorkerPool(size_t threadCount = 0);
    ~SimWorkerPool();

    SimWorkerPool(const SimWorkerPool&) = delete;
    SimWorkerPool& operator=(const SimWorkerPool&) = delete;

    /// Dispatch all tasks and block until complete.
    /// Each task receives a per-worker PRNG seeded from baseSeed + taskIndex.
    void dispatchAndWait(const std::vector<std::function<void(std::mt19937&)>>& tasks, uint64_t baseSeed);

    /// Run all tasks inline on the calling thread (deterministic, for tests).
    void disableForTesting();

    size_t threadCount() const;

  private:
    size_t threadCount_;
    bool disabled_ = false;

    void runInline(const std::vector<std::function<void(std::mt19937&)>>& tasks, uint64_t baseSeed);
};

} // namespace recurse::simulation
