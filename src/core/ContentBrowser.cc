#include "fabric/core/ContentBrowser.hh"

#include "fabric/core/Log.hh"

#include <algorithm>
#include <filesystem>

namespace fs = std::filesystem;

namespace fabric {

const std::vector<std::string> ContentBrowser::kAllowedExtensions = {
    "json", "xml", "rml", "rcss", "sc", "toml",
};

void ContentBrowser::init(const std::string& rootPath) {
    if (rootPath.empty()) {
        FABRIC_LOG_WARN("ContentBrowser::init called with empty root path");
        return;
    }

    std::error_code ec;
    rootPath_ = fs::canonical(rootPath, ec).string();
    if (ec) {
        FABRIC_LOG_WARN("ContentBrowser: cannot resolve root path '{}': {}", rootPath, ec.message());
        rootPath_ = fs::absolute(rootPath).string();
    }

    currentPath_ = rootPath_;
    refresh();
    FABRIC_LOG_INFO("ContentBrowser initialized at '{}'", rootPath_);
}

void ContentBrowser::shutdown() {
    entries_.clear();
    rootPath_.clear();
    currentPath_.clear();
    visible_ = false;
}

void ContentBrowser::toggle() {
    visible_ = !visible_;
}

bool ContentBrowser::isVisible() const {
    return visible_;
}

void ContentBrowser::navigate(const std::string& path) {
    if (path.empty()) {
        return;
    }

    std::error_code ec;
    fs::path target = fs::canonical(path, ec);
    if (ec) {
        FABRIC_LOG_WARN("ContentBrowser: invalid path '{}': {}", path, ec.message());
        return;
    }

    // Ensure target is a directory
    if (!fs::is_directory(target, ec)) {
        FABRIC_LOG_WARN("ContentBrowser: '{}' is not a directory", path);
        return;
    }

    // Ensure target is under rootPath_
    std::string targetStr = target.string();
    if (!targetStr.starts_with(rootPath_)) {
        FABRIC_LOG_WARN("ContentBrowser: '{}' is outside root '{}'", targetStr, rootPath_);
        return;
    }

    currentPath_ = targetStr;
    refresh();
}

void ContentBrowser::navigateUp() {
    if (currentPath_.empty() || currentPath_ == rootPath_) {
        return;
    }

    fs::path parent = fs::path(currentPath_).parent_path();
    std::string parentStr = parent.string();

    // Don't go above root
    if (parentStr.size() < rootPath_.size()) {
        currentPath_ = rootPath_;
    } else {
        currentPath_ = parentStr;
    }

    refresh();
}

const std::string& ContentBrowser::currentPath() const {
    return currentPath_;
}

const std::vector<ContentBrowser::Entry>& ContentBrowser::entries() const {
    return entries_;
}

void ContentBrowser::refresh() {
    entries_.clear();

    if (currentPath_.empty()) {
        return;
    }

    std::error_code ec;
    if (!fs::exists(currentPath_, ec) || !fs::is_directory(currentPath_, ec)) {
        return;
    }

    for (const auto& dirEntry : fs::directory_iterator(currentPath_, ec)) {
        if (ec) {
            break;
        }

        Entry entry;
        entry.name = dirEntry.path().filename().string();

        if (dirEntry.is_directory(ec)) {
            entry.isDirectory = true;
            entry.sizeBytes = 0;
            entries_.push_back(std::move(entry));
        } else if (dirEntry.is_regular_file(ec)) {
            std::string ext = dirEntry.path().extension().string();
            if (!ext.empty() && ext[0] == '.') {
                ext = ext.substr(1);
            }

            // Filter by allowed extensions
            bool allowed = false;
            for (const auto& ae : kAllowedExtensions) {
                if (ae == ext) {
                    allowed = true;
                    break;
                }
            }

            if (!allowed) {
                continue;
            }

            entry.extension = ext;
            entry.sizeBytes = static_cast<std::size_t>(dirEntry.file_size(ec));
            entries_.push_back(std::move(entry));
        }
    }

    // Sort: directories first, then alphabetical within each group
    std::sort(entries_.begin(), entries_.end(), [](const Entry& a, const Entry& b) {
        if (a.isDirectory != b.isDirectory) {
            return a.isDirectory > b.isDirectory; // dirs first
        }
        return a.name < b.name;
    });
}

} // namespace fabric
