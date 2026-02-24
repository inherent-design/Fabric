#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <future>
#include <iostream>
#include <mutex>
#include <optional>
#include <queue>
#include <stdexcept>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>

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
 * This class provides a reusable thread pool implementation for background tasks
 * with proper handling of thread lifecycle, task scheduling, and graceful shutdown.
 * It also includes testing support for deterministic behavior in tests.
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
     * @brief Destructor that ensures proper thread cleanup
     */
    ~ThreadPoolExecutor();

    /**
     * @brief ThreadPoolExecutor is not copyable
     */
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
     * @param count Number of worker threads (must be at least 1)
     * @throws std::invalid_argument if count is 0
     */
    void setThreadCount(size_t count);

    /**
     * @brief Get the current number of worker threads
     *
     * @return Number of worker threads
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

        // Create a packaged task with the function and its arguments
        auto task = std::make_shared<std::packaged_task<ReturnType()>>(
            [f = std::forward<Func>(func), ... args = std::forward<Args>(args)]() mutable {
                return f(std::forward<Args>(args)...);
            });

        // Get the future from the packaged task
        std::future<ReturnType> result = task->get_future();

        // Wrap the task in a void function for the task queue
        auto taskWrapper = [task]() {
            try {
                (*task)();
            } catch (const std::exception& e) {
                // Log the exception but don't propagate it to the thread pool
                // as that would terminate the worker thread
                // Future users will see the exception when they call get()
                std::cerr << "Exception in thread pool task: " << e.what() << std::endl;
            } catch (...) {
                std::cerr << "Unknown exception in thread pool task" << std::endl;
            }
        };

        // Add the task to the queue
        {
            std::unique_lock<std::mutex> lock(queueMutex_);

            // Check if the pool is shut down
            if (shutdown_) {
                throw std::runtime_error("Cannot submit task to stopped ThreadPoolExecutor");
            }

            // Check if the pool is paused for testing
            if (pausedForTesting_) {
                // If paused, run the task immediately in this thread
                lock.unlock(); // Release the lock before running the task
                taskWrapper();
                return result;
            }

            // Add the task to the queue
            taskQueue_.emplace(std::move(taskWrapper));
        }

        // Notify a worker thread
        queueCondition_.notify_one();

        return result;
    }

    /**
     * @brief Submit a task with a timeout for execution
     *
     * If the task doesn't complete within the specified timeout, the future will
     * be set to a ThreadPoolTimeoutException.
     *
     * @tparam Func Function type
     * @tparam Args Argument types
     * @param timeout Maximum time to wait for the task to complete
     * @param func Function to execute
     * @param args Arguments to pass to the function
     * @return Future for the function's result
     */
    template <typename Func, typename... Args>
    auto submitWithTimeout(std::chrono::milliseconds timeout, Func&& func, Args&&... args)
        -> std::future<std::invoke_result_t<Func, Args...>> {
        using ReturnType = std::invoke_result_t<Func, Args...>;

        // Create a promise for the result
        auto promise = std::make_shared<std::promise<ReturnType>>();
        std::future<ReturnType> result = promise->get_future();

        // Create a packaged task that will run with a timeout
        auto task = [promise, timeout, f = std::forward<Func>(func), ... args = std::forward<Args>(args)]() mutable {
            try {
                // Create a future for the actual task
                auto innerTask =
                    std::async(std::launch::async, [&f, &args...]() { return f(std::forward<Args>(args)...); });

                // Wait for the future with a timeout
                auto status = innerTask.wait_for(timeout);

                if (status == std::future_status::timeout) {
                    // Task timed out
                    promise->set_exception(std::make_exception_ptr(ThreadPoolTimeoutException("Task timed out")));
                } else {
                    // Task completed, get the result
                    try {
                        if constexpr (std::is_same_v<ReturnType, void>) {
                            innerTask.get();
                            promise->set_value();
                        } else {
                            promise->set_value(innerTask.get());
                        }
                    } catch (...) {
                        // Forward any exceptions
                        promise->set_exception(std::current_exception());
                    }
                }
            } catch (...) {
                // Handle any exceptions
                promise->set_exception(std::current_exception());
            }
        };

        // Submit the task to the thread pool
        submit(std::move(task));

        return result;
    }

    /**
     * @brief Shutdown the thread pool
     *
     * This method stops all worker threads and waits for them to finish.
     * No new tasks can be submitted after calling this method.
     *
     * WARNING (Phase 2): The timeout parameter does not work as intended.
     * Implementation uses std::async to join threads, but std::future from
     * std::async blocks on destruction, so the timeout is effectively ignored.
     * This is the root cause of test hangs documented in CLAUDE.md. Needs
     * replacement with cooperative shutdown (condition_variable + flag).
     *
     * @param timeout Maximum time to wait for threads to finish (currently ineffective)
     * @return true if all threads were gracefully shutdown, false if timeout occurred
     */
    bool shutdown(std::chrono::milliseconds timeout = std::chrono::milliseconds(1000));

    /**
     * @brief Check if the thread pool is shut down
     *
     * @return true if the thread pool is shut down
     */
    bool isShutdown() const;

    /**
     * @brief Pause the thread pool for testing
     *
     * When paused, new tasks are executed immediately in the submitting thread
     * rather than being queued for worker threads. This allows for deterministic
     * testing without actual threading.
     */
    void pauseForTesting();

    /**
     * @brief Resume the thread pool after testing
     *
     * This method resumes normal operation after a call to pauseForTesting().
     */
    void resumeAfterTesting();

    /**
     * @brief Check if the thread pool is paused for testing
     *
     * @return true if the thread pool is paused
     */
    bool isPausedForTesting() const;

    /**
     * @brief Get the number of tasks currently in the queue
     *
     * @return Task count
     */
    size_t getQueuedTaskCount() const;

  private:
    // Worker thread function
    void workerThread();

    // Task type
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
