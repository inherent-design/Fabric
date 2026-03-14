#pragma once

#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>

namespace fabric::platform {

/// Single-thread serial executor for write operations.
/// Multiple threads submit tasks; a dedicated consumer thread
/// processes them in FIFO order. Used to serialize all SQLite
/// writer access, eliminating concurrent write contention.
class WriterQueue {
  public:
    WriterQueue();
    ~WriterQueue();

    WriterQueue(WriterQueue&& other) noexcept;
    WriterQueue& operator=(WriterQueue&& other) noexcept;

    WriterQueue(const WriterQueue&) = delete;
    WriterQueue& operator=(const WriterQueue&) = delete;

    /// Submit a callable for serial execution on the writer thread.
    void submit(std::function<void()> task);

    /// Block the caller until all currently queued tasks complete.
    void drain();

    /// Signal stop, drain remaining work, join consumer thread.
    /// Idempotent: safe to call multiple times.
    void shutdown();

  private:
    void consume();

    std::queue<std::function<void()>> queue_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::jthread thread_;
    bool stopped_ = false;
};

} // namespace fabric::platform
