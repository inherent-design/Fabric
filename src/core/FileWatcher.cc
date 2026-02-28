#include "fabric/core/FileWatcher.hh"
#include "fabric/core/Log.hh"
#include <efsw/efsw.hpp>
#include <filesystem>

namespace fabric {

// Internal listener that forwards efsw callbacks into the event queue
class FileWatcherListener : public efsw::FileWatchListener {
  public:
    explicit FileWatcherListener(FileWatcher& owner) : owner_(owner) {}

    void handleFileAction(efsw::WatchID /*watchid*/, const std::string& dir, const std::string& filename,
                          efsw::Action action, std::string /*oldFilename*/) override {
        // Only handle modifications (not creates/deletes/moves for now)
        if (action != efsw::Actions::Modified) {
            return;
        }

        std::filesystem::path fullPath = std::filesystem::path(dir) / filename;
        FileChangeEvent event;
        event.directory = dir;
        event.filename = filename;
        event.fullPath = fullPath.string();
        event.timestamp = std::chrono::steady_clock::now();

        owner_.enqueueEvent(std::move(event));
    }

  private:
    FileWatcher& owner_;
};

FileWatcher::FileWatcher() = default;

FileWatcher::~FileWatcher() {
    if (valid_) {
        shutdown();
    }
}

void FileWatcher::init() {
    if (valid_) {
        return;
    }

    watcher_ = std::make_unique<efsw::FileWatcher>();
    listener_ = std::make_unique<FileWatcherListener>(*this);
    watcher_->watch();
    valid_ = true;

    FABRIC_LOG_INFO("FileWatcher initialized");
}

void FileWatcher::shutdown() {
    if (!valid_) {
        return;
    }

    valid_ = false;
    watcher_.reset();
    listener_.reset();

    {
        std::lock_guard<std::mutex> lock(eventMutex_);
        pendingEvents_.clear();
    }
    {
        std::lock_guard<std::mutex> lock(resourceMutex_);
        resources_.clear();
    }

    lastEventTime_.clear();
    extensionFilter_.clear();

    FABRIC_LOG_INFO("FileWatcher shut down");
}

bool FileWatcher::isValid() const {
    return valid_;
}

void FileWatcher::watchDirectory(const std::string& dir) {
    if (!valid_) {
        FABRIC_LOG_WARN("FileWatcher::watchDirectory called before init");
        return;
    }

    auto watchId = watcher_->addWatch(dir, listener_.get(), true);
    if (watchId < 0) {
        FABRIC_LOG_ERROR("Failed to watch directory: {}", dir);
        return;
    }

    FABRIC_LOG_DEBUG("Watching directory: {}", dir);
}

void FileWatcher::registerResource(const std::string& path, ValidateCallback validate, SwapCallback swap) {
    std::lock_guard<std::mutex> lock(resourceMutex_);
    resources_[path] = WatchedResource{std::move(validate), std::move(swap)};
}

void FileWatcher::unregisterResource(const std::string& path) {
    std::lock_guard<std::mutex> lock(resourceMutex_);
    resources_.erase(path);
}

void FileWatcher::setExtensionFilter(const std::vector<std::string>& extensions) {
    extensionFilter_.clear();
    for (const auto& ext : extensions) {
        // Normalize: ensure leading dot
        if (!ext.empty() && ext[0] != '.') {
            extensionFilter_.insert("." + ext);
        } else {
            extensionFilter_.insert(ext);
        }
    }
}

void FileWatcher::enqueueEvent(FileChangeEvent event) {
    std::lock_guard<std::mutex> lock(eventMutex_);
    pendingEvents_.push_back(std::move(event));
}

void FileWatcher::poll() {
    if (!valid_) {
        return;
    }

    // Drain pending events under lock
    std::vector<FileChangeEvent> events;
    {
        std::lock_guard<std::mutex> lock(eventMutex_);
        events.swap(pendingEvents_);
    }

    auto now = std::chrono::steady_clock::now();

    for (const auto& event : events) {
        // Extension filter check
        if (!extensionFilter_.empty() && !matchesExtensionFilter(event.filename)) {
            continue;
        }

        // Debounce: skip if within window of last event for this path
        auto it = lastEventTime_.find(event.fullPath);
        if (it != lastEventTime_.end()) {
            if ((event.timestamp - it->second) < kDebounceWindow) {
                continue;
            }
        }
        lastEventTime_[event.fullPath] = event.timestamp;

        // Look up registered resource and fire callbacks
        // Look up registered resource and copy callbacks
        ValidateCallback validateCb;
        SwapCallback swapCb;
        {
            std::lock_guard<std::mutex> lock(resourceMutex_);
            auto resIt = resources_.find(event.fullPath);
            if (resIt == resources_.end()) {
                continue;
            }
            validateCb = resIt->second.validate;
            swapCb = resIt->second.swap;
        }

        // Validate first; skip swap if validation fails
        if (validateCb && !validateCb(event.fullPath)) {
            FABRIC_LOG_WARN("Hot-reload validation failed for: {}", event.fullPath);
            continue;
        }

        if (swapCb) {
            FABRIC_LOG_INFO("Hot-reloading: {}", event.fullPath);
            swapCb(event.fullPath);
        }
    }
}

bool FileWatcher::matchesExtensionFilter(const std::string& filename) const {
    std::filesystem::path p(filename);
    std::string ext = p.extension().string();
    return extensionFilter_.find(ext) != extensionFilter_.end();
}

} // namespace fabric
