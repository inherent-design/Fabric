#include "fabric/platform/WriterQueue.hh"
#include "fabric/log/Log.hh"
#include <future>
#include <utility>

namespace fabric::platform {

WriterQueue::WriterQueue() : thread_([this](std::stop_token) { consume(); }) {}

WriterQueue::~WriterQueue() {
    shutdown();
}

WriterQueue::WriterQueue(WriterQueue&& other) noexcept {
    std::lock_guard lock(other.mutex_);
    queue_ = std::move(other.queue_);
    stopped_ = other.stopped_;
    // Thread is not movable after construction; the moved-from
    // instance will be in a valid-but-empty state.
    // The new instance starts its own consumer thread.
    if (!stopped_) {
        thread_ = std::jthread([this](std::stop_token) { consume(); });
    }
}

WriterQueue& WriterQueue::operator=(WriterQueue&& other) noexcept {
    if (this != &other) {
        shutdown();
        std::lock_guard lock(other.mutex_);
        queue_ = std::move(other.queue_);
        stopped_ = other.stopped_;
        if (!stopped_) {
            thread_ = std::jthread([this](std::stop_token) { consume(); });
        }
    }
    return *this;
}

void WriterQueue::submit(std::function<void()> task) {
    std::lock_guard lock(mutex_);
    if (stopped_) {
        FABRIC_LOG_WARN("WriterQueue::submit called after shutdown; task discarded");
        return;
    }
    queue_.push(std::move(task));
    cv_.notify_one();
}

void WriterQueue::drain() {
    std::promise<void> barrier;
    auto future = barrier.get_future();
    {
        std::lock_guard lock(mutex_);
        if (stopped_) {
            return;
        }
        queue_.push([&barrier]() { barrier.set_value(); });
        cv_.notify_one();
    }
    future.wait();
}

void WriterQueue::shutdown() {
    {
        std::lock_guard lock(mutex_);
        if (stopped_) {
            return;
        }
        stopped_ = true;
        cv_.notify_one();
    }
    if (thread_.joinable()) {
        thread_.join();
    }
}

void WriterQueue::consume() {
    while (true) {
        std::function<void()> task;
        {
            std::unique_lock lock(mutex_);
            cv_.wait(lock, [this] { return stopped_ || !queue_.empty(); });
            if (stopped_ && queue_.empty()) {
                return;
            }
            task = std::move(queue_.front());
            queue_.pop();
        }
        try {
            task();
        } catch (const std::exception& e) {
            FABRIC_LOG_ERROR("WriterQueue task threw: {}", e.what());
        } catch (...) {
            FABRIC_LOG_ERROR("WriterQueue task threw unknown exception");
        }
    }
}

} // namespace fabric::platform
