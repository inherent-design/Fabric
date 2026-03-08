#pragma once
#include <cstddef>
#include <functional>
#include <vector>

namespace fabric {

/// Unified thread pool for parallel dispatch across simulation, meshing,
/// physics, and persistence. parallelFor() partitions work across workers
/// using the same batch strategy as the former SimWorkerPool.
///
/// Current implementation uses std::async per dispatch (same as SimWorkerPool).
/// Future: persistent jthread pool with condition-variable wakeup, once the
/// macOS jthread+CV interaction issue is resolved.
class JobScheduler {
  public:
    /// @param threadCount Number of worker threads. 0 = hardware_concurrency - 2 (min 1).
    explicit JobScheduler(size_t threadCount = 0);
    ~JobScheduler();

    JobScheduler(const JobScheduler&) = delete;
    JobScheduler& operator=(const JobScheduler&) = delete;

    /// Dispatch count jobs and block until all complete.
    /// fn receives (jobIndex, workerIndex) so callers can seed per-job PRNGs.
    void parallelFor(size_t count, std::function<void(size_t jobIdx, size_t workerIdx)> fn);

    /// Run all jobs inline on calling thread (deterministic, for tests).
    void disableForTesting();

    size_t workerCount() const;

  private:
    size_t workerCount_;
    bool disabled_ = false;

    void runInline(size_t count, const std::function<void(size_t jobIdx, size_t workerIdx)>& fn);
};

} // namespace fabric
