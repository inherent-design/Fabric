#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>

#include "fabric/utils/ErrorHandling.hh"

namespace fabric {
namespace Utils {

/**
 * @brief Exception thrown when a thread pool operation times out
 */
class ThreadPoolTimeoutException : public std::runtime_error {
  public:
    explicit ThreadPoolTimeoutException(const std::string& message) : std::runtime_error(message) {}
};

/**
 * @brief A thread pool for executing asynchronous tasks with optimal concurrency
 *
 * Worker threads are indexed at creation time to avoid data races from vector
 * iteration. shutdown() always joins threads (never detaches) to prevent UB
 * from accessing destroyed state.
 *
 * Thread safety: submit() is safe to call from any thread. setThreadCount()
 * and shutdown() must not be called concurrently with each other.
 */
class ThreadPoolExecutor {
  public:
    /**
     * @brief Construct a new ThreadPoolExecutor with the specified number of threads
     *
     * @param threadCount Number of worker threads to create (defaults to hardware concurrency)
     */
    explicit ThreadPoolExecutor(size_t threadCount = std::thread::hardware_concurrency());

    /**
     * @brief Destructor that ensures proper thread cleanup via join
     */
    ~ThreadPoolExecutor();

    ThreadPoolExecutor(const ThreadPoolExecutor&) = delete;
    ThreadPoolExecutor& operator=(const ThreadPoolExecutor&) = delete;

    /**
     * @brief ThreadPoolExecutor is not movable.
     * Worker threads capture `this`; moving would create dangling pointers.
     */
    ThreadPoolExecutor(ThreadPoolExecutor&&) = delete;
    ThreadPoolExecutor& operator=(ThreadPoolExecutor&&) = delete;

    /**
     * @brief Set the number of worker threads
     *
     * When reducing, excess workers are joined. When increasing, new workers
     * are created with sequential indices.
     *
     * @param count Number of worker threads (must be at least 1)
     * @throws FabricException if count is 0
     */
    void setThreadCount(size_t count);

    /**
     * @brief Get the current number of worker threads
     */
    size_t getThreadCount() const;

    /**
     * @brief Submit a task for execution
     *
     * @tparam Func Function type
     * @tparam Args Argument types
     * @param func Function to execute
     * @param args Arguments to pass to the function
     * @return Future for the function's result
     */
    template <typename Func, typename... Args>
    auto submit(Func&& func, Args&&... args) -> std::future<std::invoke_result_t<Func, Args...>> {
        using ReturnType = std::invoke_result_t<Func, Args...>;

        auto task = std::make_shared<std::packaged_task<ReturnType()>>(
            [f = std::forward<Func>(func), ... args = std::forward<Args>(args)]() mutable {
                return f(std::forward<Args>(args)...);
            });

        std::future<ReturnType> result = task->get_future();

        {
            std::unique_lock<std::mutex> lock(queueMutex_);

            if (shutdown_) {
                throwError("Cannot submit task to stopped ThreadPoolExecutor");
            }

            if (pausedForTesting_) {
                lock.unlock();
                (*task)();
                return result;
            }

            taskQueue_.emplace([task]() { (*task)(); });
        }

        queueCondition_.notify_one();

        return result;
    }

    /**
     * @brief Submit a task with a deadline-based timeout
     *
     * Records a deadline at submission time. If the task has not started
     * executing by the deadline, the returned future receives a
     * ThreadPoolTimeoutException. Once a task begins executing, it runs
     * to completion (cooperative cancellation is not enforced).
     *
     * @tparam Func Function type
     * @tparam Args Argument types
     * @param timeout Maximum time the task may wait in the queue
     * @param func Function to execute
     * @param args Arguments to pass to the function
     * @return Future for the function's result
     */
    template <typename Func, typename... Args>
    auto submitWithTimeout(std::chrono::milliseconds timeout, Func&& func, Args&&... args)
        -> std::future<std::invoke_result_t<Func, Args...>> {
        using ReturnType = std::invoke_result_t<Func, Args...>;

        auto promise = std::make_shared<std::promise<ReturnType>>();
        auto result = promise->get_future();
        auto deadline = std::chrono::steady_clock::now() + timeout;

        submit([promise, deadline, f = std::forward<Func>(func), ... args = std::forward<Args>(args)]() mutable {
            // Check if we've exceeded the deadline before starting work
            if (std::chrono::steady_clock::now() > deadline) {
                promise->set_exception(
                    std::make_exception_ptr(ThreadPoolTimeoutException("Task timed out waiting in queue")));
                return;
            }

            try {
                if constexpr (std::is_same_v<ReturnType, void>) {
                    f(std::forward<Args>(args)...);
                    promise->set_value();
                } else {
                    promise->set_value(f(std::forward<Args>(args)...));
                }
            } catch (...) {
                promise->set_exception(std::current_exception());
            }
        });

        return result;
    }

    /**
     * @brief Shutdown the thread pool
     *
     * Sets the stop flag, wakes all workers, and joins every thread.
     * Never detaches threads. If a worker is stuck in a long task,
     * join() blocks until that task completes. The timeout parameter
     * controls when a warning is logged, but joining is unconditional.
     *
     * @param timeout Advisory duration after which a warning is logged
     * @return true if all threads joined within the timeout, false otherwise
     */
    bool shutdown(std::chrono::milliseconds timeout = std::chrono::milliseconds(5000));

    /**
     * @brief Check if the thread pool is shut down
     */
    bool isShutdown() const;

    /**
     * @brief Pause the thread pool for testing
     *
     * Stops all worker threads (joined), drains queued tasks by running
     * them inline, and switches submit() to run tasks synchronously
     * in the calling thread.
     */
    void pauseForTesting();

    /**
     * @brief Resume the thread pool after testing
     *
     * Creates fresh worker threads and restores asynchronous task execution.
     */
    void resumeAfterTesting();

    /**
     * @brief Check if the thread pool is paused for testing
     */
    bool isPausedForTesting() const;

    /**
     * @brief Get the number of tasks currently in the queue
     */
    size_t getQueuedTaskCount() const;

  private:
    void workerThread(size_t index);

    using Task = std::function<void()>;

    // Thread management
    std::vector<std::thread> workerThreads_;
    std::atomic<size_t> threadCount_;

    // Task queue
    std::queue<Task> taskQueue_;
    mutable std::mutex queueMutex_;
    std::condition_variable queueCondition_;

    // State
    std::atomic<bool> shutdown_{false};
    std::atomic<bool> pausedForTesting_{false};
};

} // namespace Utils
} // namespace fabric
