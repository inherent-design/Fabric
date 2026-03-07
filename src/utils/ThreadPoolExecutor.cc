#include "fabric/utils/ThreadPoolExecutor.hh"
#include "fabric/core/Log.hh"

namespace fabric {
namespace utils {

ThreadPoolExecutor::ThreadPoolExecutor(size_t threadCount)
    : threadCount_(threadCount > 0 ? threadCount : std::thread::hardware_concurrency()) {
    workerThreads_.reserve(threadCount_);
    for (size_t i = 0; i < threadCount_; ++i) {
        workerThreads_.emplace_back(&ThreadPoolExecutor::workerThread, this, i);
    }

    FABRIC_LOG_DEBUG("ThreadPoolExecutor created with {} threads", threadCount_.load());
}

ThreadPoolExecutor::~ThreadPoolExecutor() {
    if (!shutdown_) {
        try {
            shutdown(std::chrono::milliseconds(2000));
        } catch (const std::exception& e) {
            FABRIC_LOG_ERROR("Error during ThreadPoolExecutor shutdown: {}", e.what());
        } catch (...) {
            FABRIC_LOG_ERROR("Unknown error during ThreadPoolExecutor shutdown");
        }
    }
}

void ThreadPoolExecutor::setThreadCount(size_t count) {
    if (count == 0) {
        throwError("Thread count must be at least 1");
    }

    size_t oldCount = threadCount_.load();
    threadCount_ = count;

    if (count < oldCount) {
        // Wake excess workers so they see the reduced threadCount_ and exit
        {
            std::lock_guard<std::mutex> lock(queueMutex_);
            queueCondition_.notify_all();
        }

        // Join threads with indices >= count (they will exit their loop)
        for (size_t i = count; i < workerThreads_.size(); ++i) {
            if (workerThreads_[i].joinable()) {
                workerThreads_[i].join();
            }
        }
        workerThreads_.resize(count);
    } else if (count > oldCount && !shutdown_ && !pausedForTesting_) {
        workerThreads_.reserve(count);
        for (size_t i = oldCount; i < count; ++i) {
            workerThreads_.emplace_back(&ThreadPoolExecutor::workerThread, this, i);
        }
    }

    FABRIC_LOG_DEBUG("ThreadPoolExecutor thread count changed from {} to {}", oldCount, count);
}

size_t ThreadPoolExecutor::getThreadCount() const {
    return threadCount_;
}

bool ThreadPoolExecutor::shutdown(std::chrono::milliseconds timeout) {
    shutdown_ = true;

    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        queueCondition_.notify_all();
    }

    auto startTime = std::chrono::steady_clock::now();
    bool allJoinedInTime = true;

    for (auto& thread : workerThreads_) {
        if (!thread.joinable()) {
            continue;
        }

        if (allJoinedInTime) {
            auto elapsed =
                std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startTime);
            if (elapsed >= timeout) {
                allJoinedInTime = false;
                FABRIC_LOG_WARN("ThreadPoolExecutor shutdown: timeout exceeded, still joining remaining threads");
            }
        }

        // Always join. Blocking is preferable to the UB caused by detaching
        // threads that reference destroyed state (task queue, mutex, cv).
        thread.join();
    }
    workerThreads_.clear();

    // Drain the task queue
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        std::queue<Task> emptyQueue;
        std::swap(taskQueue_, emptyQueue);
    }

    if (!allJoinedInTime) {
        FABRIC_LOG_WARN("ThreadPoolExecutor shutdown completed (exceeded timeout)");
    } else {
        FABRIC_LOG_DEBUG("ThreadPoolExecutor shut down successfully");
    }

    return allJoinedInTime;
}

bool ThreadPoolExecutor::isShutdown() const {
    return shutdown_;
}

void ThreadPoolExecutor::pauseForTesting() {
    if (pausedForTesting_) {
        return;
    }

    std::vector<Task> pendingTasks;
    {
        std::unique_lock<std::mutex> lock(queueMutex_);
        pausedForTesting_ = true;

        while (!taskQueue_.empty()) {
            pendingTasks.push_back(std::move(taskQueue_.front()));
            taskQueue_.pop();
        }

        // Wake all workers so they see pausedForTesting_ and exit
        queueCondition_.notify_all();
    }

    // Execute drained tasks without holding the mutex
    for (auto& task : pendingTasks) {
        try {
            task();
        } catch (const std::exception& e) {
            FABRIC_LOG_ERROR("Exception in task during pauseForTesting: {}", e.what());
        } catch (...) {
            FABRIC_LOG_ERROR("Unknown exception in task during pauseForTesting");
        }
    }

    // Join exiting worker threads
    for (auto& t : workerThreads_) {
        if (t.joinable()) {
            t.join();
        }
    }
    workerThreads_.clear();

    FABRIC_LOG_DEBUG("ThreadPoolExecutor paused for testing");
}

void ThreadPoolExecutor::resumeAfterTesting() {
    if (!pausedForTesting_) {
        return;
    }

    pausedForTesting_ = false;

    if (!shutdown_) {
        workerThreads_.reserve(threadCount_);
        for (size_t i = 0; i < threadCount_; ++i) {
            workerThreads_.emplace_back(&ThreadPoolExecutor::workerThread, this, i);
        }
    }

    FABRIC_LOG_DEBUG("ThreadPoolExecutor resumed after testing");
}

bool ThreadPoolExecutor::isPausedForTesting() const {
    return pausedForTesting_;
}

size_t ThreadPoolExecutor::getQueuedTaskCount() const {
    std::lock_guard<std::mutex> lock(queueMutex_);
    return taskQueue_.size();
}

void ThreadPoolExecutor::workerThread(size_t index) {
    while (!shutdown_) {
        // Workers with index >= threadCount_ exit for dynamic downsizing
        if (index >= threadCount_) {
            break;
        }

        Task task;
        bool hasTask = false;
        {
            std::unique_lock<std::mutex> lock(queueMutex_);

            queueCondition_.wait(lock, [this, index] {
                return !taskQueue_.empty() || shutdown_ || pausedForTesting_ || index >= threadCount_;
            });

            if (shutdown_ || pausedForTesting_ || index >= threadCount_) {
                break;
            }

            if (!taskQueue_.empty()) {
                task = std::move(taskQueue_.front());
                taskQueue_.pop();
                hasTask = true;
            }
        }

        if (hasTask) {
            try {
                task();
            } catch (const std::exception& e) {
                FABRIC_LOG_ERROR("Exception in worker thread task: {}", e.what());
            } catch (...) {
                FABRIC_LOG_ERROR("Unknown exception in worker thread task");
            }
        }
    }
}

} // namespace utils
} // namespace fabric
