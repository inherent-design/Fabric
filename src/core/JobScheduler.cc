#include "fabric/core/JobScheduler.hh"
#include "fabric/utils/Profiler.hh"
#include <algorithm>
#include <future>
#include <thread>

namespace fabric {

JobScheduler::JobScheduler(size_t threadCount) {
    if (threadCount == 0) {
        auto hw = std::thread::hardware_concurrency();
        workerCount_ = (hw > 2) ? (hw - 2) : 1;
    } else {
        workerCount_ = threadCount;
    }
}

JobScheduler::~JobScheduler() = default;

void JobScheduler::disableForTesting() {
    disabled_ = true;
}

size_t JobScheduler::workerCount() const {
    return workerCount_;
}

void JobScheduler::runInline(size_t count, const std::function<void(size_t jobIdx, size_t workerIdx)>& fn) {
    for (size_t i = 0; i < count; ++i)
        fn(i, 0);
}

void JobScheduler::parallelFor(size_t count, std::function<void(size_t jobIdx, size_t workerIdx)> fn) {
    FABRIC_ZONE_SCOPED_N("job_scheduler_dispatch");

    if (count == 0)
        return;

    if (disabled_ || count == 1 || workerCount_ <= 1) {
        runInline(count, fn);
        return;
    }

    size_t numWorkers = std::min(workerCount_, count);
    std::vector<std::future<void>> futures;
    futures.reserve(numWorkers);

    size_t jobsPerWorker = count / numWorkers;
    size_t remainder = count % numWorkers;

    size_t offset = 0;
    for (size_t w = 0; w < numWorkers; ++w) {
        size_t batchSize = jobsPerWorker + (w < remainder ? 1 : 0);
        size_t batchStart = offset;
        offset += batchSize;

        futures.push_back(std::async(std::launch::async, [&fn, w, batchStart, batchSize]() {
            for (size_t i = batchStart; i < batchStart + batchSize; ++i)
                fn(i, w);
        }));
    }

    for (auto& f : futures)
        f.get();
}

} // namespace fabric
