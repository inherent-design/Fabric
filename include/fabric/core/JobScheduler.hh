#pragma once
#include <cstddef>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <type_traits>
#include <vector>

namespace enki {
class TaskScheduler;
}

namespace fabric {

/// Unified thread pool for parallel dispatch across simulation, meshing,
/// physics, and persistence. parallelFor() partitions work across workers
/// using the same batch strategy as the former SimWorkerPool.
///
/// Backend: enkiTS persistent thread pool with per-thread work-stealing.
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

    /// Submit a callable for async execution. Returns a future with the result.
    template <typename F> auto submit(F&& fn) -> std::future<std::invoke_result_t<F>>;

    /// Submit fire-and-forget work at low priority (persistence, LOD gen).
    void submitBackground(std::function<void()> fn);

    size_t workerCount() const;

  private:
    std::unique_ptr<enki::TaskScheduler> scheduler_;
    size_t workerCount_;
    bool disabled_ = false;

    void runInline(size_t count, const std::function<void(size_t jobIdx, size_t workerIdx)>& fn);
    void submitAsync(std::function<void()> work, bool background);

    struct PendingTask;
    std::mutex pendingMutex_;
    std::vector<std::unique_ptr<PendingTask>> pendingTasks_;
};

template <typename F> auto JobScheduler::submit(F&& fn) -> std::future<std::invoke_result_t<F>> {
    using R = std::invoke_result_t<F>;
    auto promise = std::make_shared<std::promise<R>>();
    auto future = promise->get_future();

    if (disabled_) {
        try {
            if constexpr (std::is_void_v<R>) {
                fn();
                promise->set_value();
            } else {
                promise->set_value(fn());
            }
        } catch (...) {
            promise->set_exception(std::current_exception());
        }
        return future;
    }

    submitAsync(
        [promise, f = std::forward<F>(fn)]() mutable {
            try {
                if constexpr (std::is_void_v<R>) {
                    f();
                    promise->set_value();
                } else {
                    promise->set_value(f());
                }
            } catch (...) {
                promise->set_exception(std::current_exception());
            }
        },
        false);

    return future;
}

} // namespace fabric
