#include "recurse/simulation/SimWorkerPool.hh"
#include "fabric/utils/Profiler.hh"
#include <algorithm>
#include <future>
#include <thread>

namespace recurse::simulation {

SimWorkerPool::SimWorkerPool(size_t threadCount) {
    if (threadCount == 0) {
        auto hw = std::thread::hardware_concurrency();
        threadCount_ = (hw > 2) ? (hw - 2) : 1;
    } else {
        threadCount_ = threadCount;
    }
}

SimWorkerPool::~SimWorkerPool() = default;

void SimWorkerPool::disableForTesting() {
    disabled_ = true;
}

size_t SimWorkerPool::threadCount() const {
    return disabled_ ? 0 : threadCount_;
}

void SimWorkerPool::runInline(const std::vector<std::function<void(std::mt19937&)>>& tasks, uint64_t baseSeed) {
    for (size_t i = 0; i < tasks.size(); ++i) {
        std::mt19937 rng(baseSeed + i);
        tasks[i](rng);
    }
}

void SimWorkerPool::dispatchAndWait(const std::vector<std::function<void(std::mt19937&)>>& tasks, uint64_t baseSeed) {
    FABRIC_ZONE_SCOPED_N("sim_pool_dispatch");

    if (tasks.empty())
        return;

    if (disabled_ || tasks.size() == 1 || threadCount_ <= 1) {
        runInline(tasks, baseSeed);
        return;
    }

    // Dispatch tasks across worker threads using std::async
    // Each task gets its own PRNG seeded from baseSeed + taskIndex
    size_t numWorkers = std::min(threadCount_, tasks.size());
    std::vector<std::future<void>> futures;
    futures.reserve(numWorkers);

    // Partition tasks into batches per worker
    size_t tasksPerWorker = tasks.size() / numWorkers;
    size_t remainder = tasks.size() % numWorkers;

    size_t taskOffset = 0;
    for (size_t w = 0; w < numWorkers; ++w) {
        size_t batchSize = tasksPerWorker + (w < remainder ? 1 : 0);
        size_t batchStart = taskOffset;
        taskOffset += batchSize;

        futures.push_back(std::async(std::launch::async, [&tasks, baseSeed, batchStart, batchSize]() {
            for (size_t i = batchStart; i < batchStart + batchSize; ++i) {
                std::mt19937 rng(baseSeed + i);
                tasks[i](rng);
            }
        }));
    }

    // Wait for all workers
    for (auto& f : futures)
        f.get();
}

} // namespace recurse::simulation
