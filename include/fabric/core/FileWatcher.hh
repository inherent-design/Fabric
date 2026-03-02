#pragma once

#include <chrono>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace efsw {
class FileWatcher;
class FileWatchListener;
} // namespace efsw

namespace fabric {

// File change event queued for main-thread processing
struct FileChangeEvent {
    std::string directory;
    std::string filename;
    std::string fullPath;
    std::chrono::steady_clock::time_point timestamp;
};

class FileWatcher {
  public:
    using ValidateCallback = std::function<bool(const std::string& path)>;
    using SwapCallback = std::function<void(const std::string& path)>;

    FileWatcher();
    ~FileWatcher();

    // Lifecycle
    void init();
    void shutdown();
    bool isValid() const;

    // Watch a directory for file changes
    void watchDirectory(const std::string& dir);

    // Register a resource for hot-reload notification
    void registerResource(const std::string& path, ValidateCallback validate, SwapCallback swap);
    void unregisterResource(const std::string& path);

    // Only trigger callbacks for files with these extensions (e.g., ".glsl", ".so")
    void setExtensionFilter(const std::vector<std::string>& extensions);

    // Process pending events on the main thread. Call once per frame.
    void poll();

    // Queue an event (called from efsw listener thread)
    void enqueueEvent(FileChangeEvent event);

    FileWatcher(const FileWatcher&) = delete;
    FileWatcher& operator=(const FileWatcher&) = delete;

  private:
    bool matchesExtensionFilter(const std::string& filename) const;

    struct WatchedResource {
        ValidateCallback validate;
        SwapCallback swap;
    };

    bool valid_ = false;

    std::unique_ptr<efsw::FileWatcher> watcher_;
    std::unique_ptr<efsw::FileWatchListener> listener_;

    mutable std::mutex eventMutex_;
    std::vector<FileChangeEvent> pendingEvents_;

    mutable std::mutex resourceMutex_;
    std::unordered_map<std::string, WatchedResource> resources_;

    std::unordered_set<std::string> extensionFilter_;

    // Debounce: track last event timestamp per path
    std::unordered_map<std::string, std::chrono::steady_clock::time_point> lastEventTime_;
    static constexpr std::chrono::milliseconds kDebounceWindow{100};
};

} // namespace fabric
