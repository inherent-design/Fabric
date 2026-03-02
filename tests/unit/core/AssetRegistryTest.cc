#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

#include "fabric/core/AssetRegistry.hh"
#include "fabric/core/ResourceHub.hh"

namespace fabric {

// Test asset with controllable memory reporting
struct TestAsset {
    std::string data;
    size_t reportedSize = 64;
};

// Loader that reads file content into TestAsset
class TestAssetLoader : public AssetLoader<TestAsset> {
  public:
    bool shouldFail = false;

    std::unique_ptr<TestAsset> load(const std::filesystem::path& path, ResourceHub& /*hub*/) override {
        if (shouldFail)
            throw std::runtime_error("Intentional test failure");
        auto asset = std::make_unique<TestAsset>();
        std::ifstream ifs(path);
        if (ifs.is_open()) {
            std::getline(ifs, asset->data);
        } else {
            asset->data = path.filename().string();
        }
        return asset;
    }

    std::vector<std::string> extensions() const override { return {".test", ".dat"}; }
};

// Second type to verify type isolation
struct OtherAsset {
    int value = 0;
};

class OtherAssetLoader : public AssetLoader<OtherAsset> {
  public:
    std::unique_ptr<OtherAsset> load(const std::filesystem::path& /*path*/, ResourceHub& /*hub*/) override {
        auto asset = std::make_unique<OtherAsset>();
        asset->value = 42;
        return asset;
    }

    std::vector<std::string> extensions() const override { return {".other"}; }
};

class AssetRegistryTest : public ::testing::Test {
  protected:
    ResourceHub hub;
    std::filesystem::path tempDir;

    void SetUp() override {
        hub.disableWorkerThreadsForTesting();
        tempDir = std::filesystem::temp_directory_path() / "fabric_asset_test";
        std::filesystem::create_directories(tempDir);
    }

    void TearDown() override { std::filesystem::remove_all(tempDir); }

    std::filesystem::path writeTempFile(const std::string& name, const std::string& content) {
        auto path = tempDir / name;
        std::ofstream ofs(path);
        ofs << content;
        ofs.close();
        return path;
    }
};

// -- Loader registration --

TEST_F(AssetRegistryTest, RegisterLoaderAndLoad) {
    AssetRegistry registry(hub);
    registry.registerLoader<TestAsset>(std::make_unique<TestAssetLoader>());

    auto path = writeTempFile("hello.test", "hello world");
    auto handle = registry.load<TestAsset>(path);

    EXPECT_TRUE(handle.isLoaded());
    EXPECT_EQ(handle.get().data, "hello world");
}

TEST_F(AssetRegistryTest, LoadWithoutRegisteredLoaderThrows) {
    AssetRegistry registry(hub);
    auto path = writeTempFile("no_loader.test", "data");

    EXPECT_THROW(registry.load<TestAsset>(path), std::exception);
}

// -- Deduplication --

TEST_F(AssetRegistryTest, DeduplicatesByPath) {
    AssetRegistry registry(hub);
    registry.registerLoader<TestAsset>(std::make_unique<TestAssetLoader>());

    auto path = writeTempFile("dup.test", "content");
    auto h1 = registry.load<TestAsset>(path);
    auto h2 = registry.load<TestAsset>(path);

    EXPECT_EQ(h1, h2);
    EXPECT_EQ(registry.count<TestAsset>(), 1u);
}

// -- Handle state transitions --

TEST_F(AssetRegistryTest, HandleStateTransitions) {
    AssetRegistry registry(hub);
    registry.registerLoader<TestAsset>(std::make_unique<TestAssetLoader>());

    auto path = writeTempFile("state.test", "data");
    auto handle = registry.load<TestAsset>(path);

    EXPECT_EQ(handle.state(), AssetState::Loaded);
    EXPECT_TRUE(handle.isLoaded());
    EXPECT_FALSE(handle.isFailed());
    EXPECT_NE(handle.tryGet(), nullptr);
}

TEST_F(AssetRegistryTest, NullHandleState) {
    Handle<TestAsset> h;
    EXPECT_EQ(h.state(), AssetState::Unloaded);
    EXPECT_FALSE(h.isLoaded());
    EXPECT_EQ(h.tryGet(), nullptr);
    EXPECT_FALSE(static_cast<bool>(h));
}

// -- Callbacks --

TEST_F(AssetRegistryTest, OnLoadedCallbackFiresImmediately) {
    AssetRegistry registry(hub);
    registry.registerLoader<TestAsset>(std::make_unique<TestAssetLoader>());

    auto path = writeTempFile("cb.test", "callback data");
    auto handle = registry.load<TestAsset>(path);

    bool called = false;
    std::string captured;
    handle.onLoaded([&](TestAsset& asset) {
        called = true;
        captured = asset.data;
    });

    EXPECT_TRUE(called);
    EXPECT_EQ(captured, "callback data");
}

// -- Unload --

TEST_F(AssetRegistryTest, UnloadByHandle) {
    AssetRegistry registry(hub);
    registry.registerLoader<TestAsset>(std::make_unique<TestAssetLoader>());

    auto path = writeTempFile("unload.test", "data");
    auto handle = registry.load<TestAsset>(path);
    EXPECT_EQ(registry.count<TestAsset>(), 1u);

    registry.unload(handle);
    EXPECT_EQ(registry.count<TestAsset>(), 0u);
}

TEST_F(AssetRegistryTest, UnloadByPath) {
    AssetRegistry registry(hub);
    registry.registerLoader<TestAsset>(std::make_unique<TestAssetLoader>());

    auto path = writeTempFile("unload_path.test", "data");
    registry.load<TestAsset>(path);
    EXPECT_EQ(registry.count<TestAsset>(), 1u);

    registry.unload<TestAsset>(path);
    EXPECT_EQ(registry.count<TestAsset>(), 0u);
}

// -- Failure state --

TEST_F(AssetRegistryTest, LoadFailureSetsFailedState) {
    auto loader = std::make_unique<TestAssetLoader>();
    loader->shouldFail = true;

    AssetRegistry registry(hub);
    registry.registerLoader<TestAsset>(std::move(loader));

    auto path = writeTempFile("fail.test", "data");
    auto handle = registry.load<TestAsset>(path);

    EXPECT_TRUE(handle.isFailed());
    EXPECT_EQ(handle.state(), AssetState::Failed);
    EXPECT_FALSE(handle.error().empty());
    EXPECT_EQ(handle.tryGet(), nullptr);
}

// -- Reload --

TEST_F(AssetRegistryTest, ReloadUpdatesAssetData) {
    AssetRegistry registry(hub);
    registry.registerLoader<TestAsset>(std::make_unique<TestAssetLoader>());

    auto path = writeTempFile("reload.test", "original");
    auto handle = registry.load<TestAsset>(path);
    EXPECT_EQ(handle.get().data, "original");

    // Overwrite file
    {
        std::ofstream ofs(path);
        ofs << "updated";
    }

    registry.reload<TestAsset>(path);
    EXPECT_EQ(handle.get().data, "updated");
}

// -- Count and totalCount --

TEST_F(AssetRegistryTest, CountAndTotalCount) {
    AssetRegistry registry(hub);
    registry.registerLoader<TestAsset>(std::make_unique<TestAssetLoader>());
    registry.registerLoader<OtherAsset>(std::make_unique<OtherAssetLoader>());

    auto p1 = writeTempFile("a.test", "a");
    auto p2 = writeTempFile("b.test", "b");
    auto p3 = writeTempFile("c.other", "c");

    registry.load<TestAsset>(p1);
    registry.load<TestAsset>(p2);
    registry.load<OtherAsset>(p3);

    EXPECT_EQ(registry.count<TestAsset>(), 2u);
    EXPECT_EQ(registry.count<OtherAsset>(), 1u);
    EXPECT_EQ(registry.totalCount(), 3u);
}

// -- Type isolation --

TEST_F(AssetRegistryTest, TypeIsolation) {
    AssetRegistry registry(hub);
    registry.registerLoader<TestAsset>(std::make_unique<TestAssetLoader>());
    registry.registerLoader<OtherAsset>(std::make_unique<OtherAssetLoader>());

    auto path = writeTempFile("iso.test", "test");
    registry.load<TestAsset>(path);

    // get<OtherAsset> for the same path should return null (different type slot)
    auto other = registry.get<OtherAsset>(path);
    EXPECT_FALSE(static_cast<bool>(other));
}

// -- Get without load --

TEST_F(AssetRegistryTest, GetReturnsNullForUnloaded) {
    AssetRegistry registry(hub);
    registry.registerLoader<TestAsset>(std::make_unique<TestAssetLoader>());

    auto handle = registry.get<TestAsset>("/nonexistent/path.test");
    EXPECT_FALSE(static_cast<bool>(handle));
}

TEST_F(AssetRegistryTest, GetReturnsExistingHandle) {
    AssetRegistry registry(hub);
    registry.registerLoader<TestAsset>(std::make_unique<TestAssetLoader>());

    auto path = writeTempFile("getexist.test", "exist");
    auto loaded = registry.load<TestAsset>(path);
    auto got = registry.get<TestAsset>(path);

    EXPECT_TRUE(static_cast<bool>(got));
    EXPECT_EQ(loaded, got);
}

// -- LRU eviction --

TEST_F(AssetRegistryTest, LRUEvictionUnderBudget) {
    // TODO(human): Implement LRU eviction test strategy
    // See guidance below the test fixture
    GTEST_SKIP() << "Awaiting human implementation";
}

// -- Memory usage --

TEST_F(AssetRegistryTest, MemoryUsageTracking) {
    AssetRegistry registry(hub);
    registry.registerLoader<TestAsset>(std::make_unique<TestAssetLoader>());

    EXPECT_EQ(registry.getMemoryUsage<TestAsset>(), 0u);

    auto path = writeTempFile("mem.test", "data");
    registry.load<TestAsset>(path);

    EXPECT_GT(registry.getMemoryUsage<TestAsset>(), 0u);
}

} // namespace fabric
