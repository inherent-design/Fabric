#include "fabric/utils/ThreadPoolExecutor.hh"
#include "fabric/core/Log.hh"
#include <algorithm>
#include <iostream>

namespace fabric {
namespace Utils {

ThreadPoolExecutor::ThreadPoolExecutor(size_t threadCount)
    : threadCount_(threadCount > 0 ? threadCount : std::thread::hardware_concurrency()) {
    // Start the worker threads
    workerThreads_.reserve(threadCount_);
    for (size_t i = 0; i < threadCount_; ++i) {
        workerThreads_.emplace_back(&ThreadPoolExecutor::workerThread, this);
    }
    
    FABRIC_LOG_DEBUG("ThreadPoolExecutor created with {} threads", threadCount_.load());
}

ThreadPoolExecutor::~ThreadPoolExecutor() {
    // Shutdown the thread pool if not already done
    if (!shutdown_) {
        try {
            // Use a shorter timeout in the destructor to avoid blocking
            shutdown(std::chrono::milliseconds(200));
        } catch (const std::exception& e) {
            // Log the error but continue destruction
            FABRIC_LOG_ERROR("Error during ThreadPoolExecutor shutdown: {}", e.what());
        } catch (...) {
            FABRIC_LOG_ERROR("Unknown error during ThreadPoolExecutor shutdown");
        }
    }
}

// Move constructor and assignment deleted (see header).
// Worker threads capture `this`; moving would create dangling pointers.

void ThreadPoolExecutor::setThreadCount(size_t count) {
    if (count == 0) {
        throw std::invalid_argument("Thread count must be at least 1");
    }
    
    // Store the current thread count
    size_t oldCount = threadCount_;
    
    // Set the new thread count
    threadCount_ = count;
    
    // If we're reducing the thread count
    if (count < oldCount) {
        std::lock_guard<std::mutex> lock(queueMutex_);
        
        // Notify worker threads that they should check their status
        queueCondition_.notify_all();
        
        // The excess threads will exit naturally in workerThread()
        // when they recheck threadCount_
    }
    
    // If we're increasing the thread count
    if (count > oldCount && !shutdown_ && !pausedForTesting_) {
        // Start new worker threads
        for (size_t i = oldCount; i < count; ++i) {
            workerThreads_.emplace_back(&ThreadPoolExecutor::workerThread, this);
        }
    }
    
    FABRIC_LOG_DEBUG("ThreadPoolExecutor thread count changed from {} to {}",
                     oldCount, count);
}

size_t ThreadPoolExecutor::getThreadCount() const {
    return threadCount_;
}

bool ThreadPoolExecutor::shutdown(std::chrono::milliseconds timeout) {
    shutdown_ = true;

    // Wake all workers so they see the shutdown flag
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        queueCondition_.notify_all();
    }

    // Workers are cooperative: they check shutdown_ each iteration and exit.
    // join() should return fast. Timeout is a safety net for stuck tasks.
    auto startTime = std::chrono::steady_clock::now();
    bool allJoined = true;

    for (auto& thread : workerThreads_) {
        if (!thread.joinable()) {
            continue;
        }

        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - startTime);

        if (elapsed >= timeout) {
            // Time budget exhausted — detach remaining threads
            allJoined = false;
            break;
        }

        // Workers exit within one condition_variable cycle (~5ms).
        // join() will return quickly for cooperative threads.
        thread.join();
    }

    // Detach any threads we couldn't join in time
    for (auto& thread : workerThreads_) {
        if (thread.joinable()) {
            thread.detach();
            allJoined = false;
        }
    }
    workerThreads_.clear();

    // Drain the task queue
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        std::queue<Task> emptyQueue;
        std::swap(taskQueue_, emptyQueue);
    }

    if (!allJoined) {
        FABRIC_LOG_WARN("ThreadPoolExecutor shutdown: some threads detached after timeout");
    } else {
        FABRIC_LOG_DEBUG("ThreadPoolExecutor shut down successfully");
    }

    return allJoined;
}

bool ThreadPoolExecutor::isShutdown() const {
    return shutdown_;
}

void ThreadPoolExecutor::pauseForTesting() {
    if (pausedForTesting_) {
        return;
    }

    // Drain queued tasks under lock, then execute outside lock.
    // Using unique_lock for manual lock control instead of lock_guard
    // to avoid double-unlock UB.
    std::vector<Task> pendingTasks;
    {
        std::unique_lock<std::mutex> lock(queueMutex_);
        pausedForTesting_ = true;

        while (!taskQueue_.empty()) {
            pendingTasks.push_back(std::move(taskQueue_.front()));
            taskQueue_.pop();
        }
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

    FABRIC_LOG_DEBUG("ThreadPoolExecutor paused for testing");
}

void ThreadPoolExecutor::resumeAfterTesting() {
    // If we're not paused, do nothing
    if (!pausedForTesting_) {
        return;
    }
    
    // Resume normal operation
    pausedForTesting_ = false;
    
    // Restart worker threads if needed
    if (!shutdown_ && workerThreads_.size() < threadCount_) {
        size_t threadsToStart = threadCount_ - workerThreads_.size();
        for (size_t i = 0; i < threadsToStart; ++i) {
            workerThreads_.emplace_back(&ThreadPoolExecutor::workerThread, this);
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

void ThreadPoolExecutor::workerThread() {
    // Loop until shutdown or thread count reduced
    while (!shutdown_) {
        // Calculate this thread's index
        size_t threadIndex = 0;
        for (size_t i = 0; i < workerThreads_.size(); ++i) {
            if (workerThreads_[i].get_id() == std::this_thread::get_id()) {
                threadIndex = i;
                break;
            }
        }
        
        // Check if this thread should exit due to thread count reduction
        if (threadIndex >= threadCount_) {
            break;
        }
        
        // Get a task from the queue
        Task task;
        bool hasTask = false;
        {
            std::unique_lock<std::mutex> lock(queueMutex_);
            
            // Wait for a task or shutdown signal
            queueCondition_.wait(lock, [this, threadIndex] {
                return !taskQueue_.empty() || shutdown_ || pausedForTesting_ || threadIndex >= threadCount_;
            });
            
            // Check for shutdown or thread count reduction
            if (shutdown_ || pausedForTesting_ || threadIndex >= threadCount_) {
                break;
            }
            
            // Get the task
            if (!taskQueue_.empty()) {
                task = std::move(taskQueue_.front());
                taskQueue_.pop();
                hasTask = true;
            }
        }
        
        // Execute the task
        if (hasTask) {
            try {
                task();
            } catch (const std::exception& e) {
                // Log but don't terminate the worker thread
                FABRIC_LOG_ERROR("Exception in worker thread task: {}", e.what());
            } catch (...) {
                FABRIC_LOG_ERROR("Unknown exception in worker thread task");
            }
        }
    }
}

} // namespace Utils
} // namespace fabric