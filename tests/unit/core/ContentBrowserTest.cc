#include "fabric/core/ContentBrowser.hh"

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;
using namespace fabric;

namespace {

/// RAII helper: creates a temporary directory tree and removes it on destruction.
class TempDir {
  public:
    TempDir() {
        path_ = fs::temp_directory_path() / ("fabric_cb_test_" + std::to_string(counter_++));
        fs::create_directories(path_);
    }
    ~TempDir() {
        std::error_code ec;
        fs::remove_all(path_, ec);
    }

    TempDir(const TempDir&) = delete;
    TempDir& operator=(const TempDir&) = delete;

    const fs::path& path() const { return path_; }

    /// Create a file with optional content.
    void createFile(const std::string& relativePath, const std::string& content = "") {
        fs::path full = path_ / relativePath;
        fs::create_directories(full.parent_path());
        std::ofstream ofs(full);
        ofs << content;
    }

    /// Create a subdirectory.
    void createDir(const std::string& relativePath) { fs::create_directories(path_ / relativePath); }

  private:
    fs::path path_;
    static inline int counter_ = 0;
};

} // namespace

// ---------------------------------------------------------------------------
// Init
// ---------------------------------------------------------------------------

TEST(ContentBrowserTest, InitWithValidDirectory) {
    TempDir tmp;
    tmp.createFile("scene.json", R"({"name":"test"})");
    tmp.createFile("style.rcss", "body {}");

    ContentBrowser browser;
    browser.init(tmp.path().string());

    EXPECT_EQ(browser.currentPath(), fs::canonical(tmp.path()).string());
    EXPECT_FALSE(browser.entries().empty());
}

TEST(ContentBrowserTest, InitWithEmptyDirectory) {
    TempDir tmp;

    ContentBrowser browser;
    browser.init(tmp.path().string());

    EXPECT_TRUE(browser.entries().empty());
}

// ---------------------------------------------------------------------------
// Navigation
// ---------------------------------------------------------------------------

TEST(ContentBrowserTest, NavigateIntoSubdirectory) {
    TempDir tmp;
    tmp.createDir("models");
    tmp.createFile("models/mesh.json", "{}");

    ContentBrowser browser;
    browser.init(tmp.path().string());

    fs::path subdir = fs::canonical(tmp.path() / "models");
    browser.navigate(subdir.string());

    EXPECT_EQ(browser.currentPath(), subdir.string());
    ASSERT_EQ(browser.entries().size(), 1u);
    EXPECT_EQ(browser.entries()[0].name, "mesh.json");
}

TEST(ContentBrowserTest, NavigateUpReturnsToParent) {
    TempDir tmp;
    tmp.createDir("sub");
    tmp.createFile("root.json", "{}");

    ContentBrowser browser;
    browser.init(tmp.path().string());

    fs::path subdir = fs::canonical(tmp.path() / "sub");
    browser.navigate(subdir.string());
    EXPECT_EQ(browser.currentPath(), subdir.string());

    browser.navigateUp();
    EXPECT_EQ(browser.currentPath(), fs::canonical(tmp.path()).string());
}

TEST(ContentBrowserTest, NavigateUpAtRootStaysAtRoot) {
    TempDir tmp;

    ContentBrowser browser;
    browser.init(tmp.path().string());

    std::string rootPath = browser.currentPath();

    browser.navigateUp();
    EXPECT_EQ(browser.currentPath(), rootPath);

    // Multiple navigateUp should still stay at root
    browser.navigateUp();
    browser.navigateUp();
    EXPECT_EQ(browser.currentPath(), rootPath);
}

// ---------------------------------------------------------------------------
// File type filtering
// ---------------------------------------------------------------------------

TEST(ContentBrowserTest, FileTypeFiltering) {
    TempDir tmp;
    tmp.createFile("config.json", "{}");
    tmp.createFile("layout.rml", "<rml/>");
    tmp.createFile("style.rcss", "body{}");
    tmp.createFile("data.xml", "<data/>");
    tmp.createFile("shader.sc", "void main(){}");
    tmp.createFile("settings.toml", "[settings]");
    tmp.createFile("texture.png", "PNG");       // filtered out
    tmp.createFile("model.obj", "v 0 0 0");     // filtered out
    tmp.createFile("readme.md", "# readme");    // filtered out
    tmp.createFile("code.cpp", "int main(){}"); // filtered out

    ContentBrowser browser;
    browser.init(tmp.path().string());

    const auto& entries = browser.entries();
    // Should only have the 6 allowed extensions
    EXPECT_EQ(entries.size(), 6u);

    for (const auto& e : entries) {
        bool found = false;
        for (const auto& ext : ContentBrowser::kAllowedExtensions) {
            if (e.extension == ext) {
                found = true;
                break;
            }
        }
        EXPECT_TRUE(found) << "Unexpected extension: " << e.extension;
    }
}

// ---------------------------------------------------------------------------
// Sorting
// ---------------------------------------------------------------------------

TEST(ContentBrowserTest, EntriesSortedDirsFirstThenAlpha) {
    TempDir tmp;
    tmp.createDir("zfolder");
    tmp.createDir("afolder");
    tmp.createFile("beta.json", "{}");
    tmp.createFile("alpha.json", "{}");

    ContentBrowser browser;
    browser.init(tmp.path().string());

    const auto& entries = browser.entries();
    ASSERT_EQ(entries.size(), 4u);

    // First two should be directories (sorted: afolder, zfolder)
    EXPECT_TRUE(entries[0].isDirectory);
    EXPECT_EQ(entries[0].name, "afolder");
    EXPECT_TRUE(entries[1].isDirectory);
    EXPECT_EQ(entries[1].name, "zfolder");

    // Last two should be files (sorted: alpha.json, beta.json)
    EXPECT_FALSE(entries[2].isDirectory);
    EXPECT_EQ(entries[2].name, "alpha.json");
    EXPECT_FALSE(entries[3].isDirectory);
    EXPECT_EQ(entries[3].name, "beta.json");
}

// ---------------------------------------------------------------------------
// Toggle
// ---------------------------------------------------------------------------

TEST(ContentBrowserTest, ToggleFlipsVisibility) {
    ContentBrowser browser;
    EXPECT_FALSE(browser.isVisible());

    browser.toggle();
    EXPECT_TRUE(browser.isVisible());

    browser.toggle();
    EXPECT_FALSE(browser.isVisible());
}

// ---------------------------------------------------------------------------
// Entry struct
// ---------------------------------------------------------------------------

TEST(ContentBrowserTest, EntryStructPopulatedCorrectly) {
    TempDir tmp;
    tmp.createFile("test.json", R"({"hello":"world"})");
    tmp.createDir("subdir");

    ContentBrowser browser;
    browser.init(tmp.path().string());

    const auto& entries = browser.entries();
    ASSERT_EQ(entries.size(), 2u);

    // Directory entry
    const auto& dirEntry = entries[0];
    EXPECT_EQ(dirEntry.name, "subdir");
    EXPECT_TRUE(dirEntry.isDirectory);
    EXPECT_TRUE(dirEntry.extension.empty());
    EXPECT_EQ(dirEntry.sizeBytes, 0u);

    // File entry
    const auto& fileEntry = entries[1];
    EXPECT_EQ(fileEntry.name, "test.json");
    EXPECT_FALSE(fileEntry.isDirectory);
    EXPECT_EQ(fileEntry.extension, "json");
    EXPECT_GT(fileEntry.sizeBytes, 0u);
}

// ---------------------------------------------------------------------------
// Shutdown
// ---------------------------------------------------------------------------

TEST(ContentBrowserTest, ShutdownClearsState) {
    TempDir tmp;
    tmp.createFile("data.json", "{}");

    ContentBrowser browser;
    browser.init(tmp.path().string());
    browser.toggle();
    EXPECT_TRUE(browser.isVisible());
    EXPECT_FALSE(browser.entries().empty());

    browser.shutdown();
    EXPECT_FALSE(browser.isVisible());
    EXPECT_TRUE(browser.entries().empty());
    EXPECT_TRUE(browser.currentPath().empty());
}

// ---------------------------------------------------------------------------
// Edge cases
// ---------------------------------------------------------------------------

TEST(ContentBrowserTest, NavigateOutsideRootRejected) {
    TempDir tmp;
    tmp.createDir("inside");

    ContentBrowser browser;
    browser.init((tmp.path() / "inside").string());

    // Attempt to navigate to parent (outside root)
    browser.navigate(tmp.path().string());

    // Should stay at root
    EXPECT_EQ(browser.currentPath(), fs::canonical(tmp.path() / "inside").string());
}

TEST(ContentBrowserTest, NavigateToNonExistentPathIgnored) {
    TempDir tmp;

    ContentBrowser browser;
    browser.init(tmp.path().string());

    std::string original = browser.currentPath();
    browser.navigate("/nonexistent/path/that/does/not/exist");
    EXPECT_EQ(browser.currentPath(), original);
}
