#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace fabric {

/// Content browser for navigating engine asset directories.
/// Toggle visibility with F7. Uses std::filesystem to scan directories
/// and filter by allowed asset extensions.
class ContentBrowser {
  public:
    ContentBrowser() = default;
    ~ContentBrowser() = default;

    /// Initialize with a root asset path. Scans the root immediately.
    void init(const std::string& rootPath);

    /// Clean up internal state.
    void shutdown();

    /// Toggle panel visibility.
    void toggle();

    /// Whether the panel is currently visible.
    bool isVisible() const;

    /// Navigate into a subdirectory (must be under rootPath).
    void navigate(const std::string& path);

    /// Navigate to the parent directory (stops at rootPath).
    void navigateUp();

    /// Current browsing directory (absolute).
    const std::string& currentPath() const;

    /// Single directory entry.
    struct Entry {
        std::string name;
        bool isDirectory = false;
        std::string extension;
        std::size_t sizeBytes = 0;
    };

    /// Entries for the current directory listing.
    const std::vector<Entry>& entries() const;

    /// Allowed asset file extensions (without leading dot).
    static const std::vector<std::string> kAllowedExtensions;

  private:
    /// Re-scan the current directory and rebuild entries_.
    void refresh();

    std::string rootPath_;
    std::string currentPath_;
    std::vector<Entry> entries_;
    bool visible_ = false;
};

} // namespace fabric
