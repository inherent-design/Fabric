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
        auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
        tempDir = std::filesystem::temp_directory_path() / ("fabric_asset_test_" + std::string(info->name()));
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
    AssetRegistry registry(hub);
    registry.registerLoader<TestAsset>(std::make_unique<TestAssetLoader>());

    // Load 3 assets. Each reports sizeof(TestAsset) bytes when loaded.
    auto p1 = writeTempFile("lru1.test", "first");
    auto p2 = writeTempFile("lru2.test", "second");
    auto p3 = writeTempFile("lru3.test", "third");

    registry.load<TestAsset>(p1); // tick 0, oldest
    registry.update();            // tick becomes 1
    registry.load<TestAsset>(p2); // tick 1
    registry.update();            // tick becomes 2
    registry.load<TestAsset>(p3); // tick 2, newest

    EXPECT_EQ(registry.count<TestAsset>(), 3u);

    // Set budget to fit only 2 assets, then trigger eviction
    size_t perAsset = sizeof(TestAsset);
    registry.setMemoryBudget<TestAsset>(perAsset * 2);
    registry.update(); // tick 3, runs eviction

    // Oldest asset (p1, lastAccessTick=0) should be evicted
    EXPECT_EQ(registry.count<TestAsset>(), 2u);

    // p1 should be gone, p2 and p3 should remain
    auto h1 = registry.get<TestAsset>(p1);
    auto h2 = registry.get<TestAsset>(p2);
    auto h3 = registry.get<TestAsset>(p3);

    EXPECT_FALSE(static_cast<bool>(h1));
    EXPECT_TRUE(static_cast<bool>(h2));
    EXPECT_TRUE(static_cast<bool>(h3));
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

// ========================================================================
// Audit: Handle<T> get() on non-Loaded states
// ========================================================================

TEST_F(AssetRegistryTest, GetOnNullHandleThrows) {
    Handle<TestAsset> h;
    EXPECT_THROW(h.get(), std::exception);
}

TEST_F(AssetRegistryTest, GetOnFailedHandleThrows) {
    auto loader = std::make_unique<TestAssetLoader>();
    loader->shouldFail = true;
    AssetRegistry registry(hub);
    registry.registerLoader<TestAsset>(std::move(loader));

    auto path = writeTempFile("failget.test", "data");
    auto handle = registry.load<TestAsset>(path);

    EXPECT_TRUE(handle.isFailed());
    EXPECT_THROW(handle.get(), std::exception);
}

// ========================================================================
// Audit: Handle<T> callbacks
// ========================================================================

TEST_F(AssetRegistryTest, OnLoadedMultipleCallbacksAllFire) {
    AssetRegistry registry(hub);
    registry.registerLoader<TestAsset>(std::make_unique<TestAssetLoader>());

    auto path = writeTempFile("multi_cb.test", "data");
    auto handle = registry.load<TestAsset>(path);

    int count = 0;
    handle.onLoaded([&](TestAsset&) { ++count; });
    handle.onLoaded([&](TestAsset&) { ++count; });
    handle.onLoaded([&](TestAsset&) { ++count; });

    EXPECT_EQ(count, 3);
}

TEST_F(AssetRegistryTest, OnLoadedOnNullHandleIsNoop) {
    Handle<TestAsset> h;
    bool called = false;
    h.onLoaded([&](TestAsset&) { called = true; });
    EXPECT_FALSE(called);
}

TEST_F(AssetRegistryTest, OnLoadedOnFailedHandleDoesNotFire) {
    auto loader = std::make_unique<TestAssetLoader>();
    loader->shouldFail = true;
    AssetRegistry registry(hub);
    registry.registerLoader<TestAsset>(std::move(loader));

    auto path = writeTempFile("fail_cb.test", "data");
    auto handle = registry.load<TestAsset>(path);

    bool called = false;
    handle.onLoaded([&](TestAsset&) { called = true; });
    EXPECT_FALSE(called);
}

// ========================================================================
// Audit: Handle<T> copy/move/equality
// ========================================================================

TEST_F(AssetRegistryTest, HandleCopySharesUnderlyingAsset) {
    AssetRegistry registry(hub);
    registry.registerLoader<TestAsset>(std::make_unique<TestAssetLoader>());

    auto path = writeTempFile("copy.test", "shared");
    auto h1 = registry.load<TestAsset>(path);
    auto h2 = h1;

    EXPECT_EQ(h1, h2);
    EXPECT_EQ(&h1.get(), &h2.get());
}

TEST_F(AssetRegistryTest, HandleMoveTransfersOwnership) {
    AssetRegistry registry(hub);
    registry.registerLoader<TestAsset>(std::make_unique<TestAssetLoader>());

    auto path = writeTempFile("move.test", "data");
    auto h1 = registry.load<TestAsset>(path);
    auto* ptr = h1.tryGet();

    auto h2 = std::move(h1);

    EXPECT_FALSE(static_cast<bool>(h1));
    EXPECT_TRUE(static_cast<bool>(h2));
    EXPECT_EQ(h2.tryGet(), ptr);
}

TEST_F(AssetRegistryTest, HandleInequalityForDifferentAssets) {
    AssetRegistry registry(hub);
    registry.registerLoader<TestAsset>(std::make_unique<TestAssetLoader>());

    auto p1 = writeTempFile("neq1.test", "a");
    auto p2 = writeTempFile("neq2.test", "b");

    auto h1 = registry.load<TestAsset>(p1);
    auto h2 = registry.load<TestAsset>(p2);

    EXPECT_NE(h1, h2);
}

// ========================================================================
// Audit: Handle<T> accessor edge cases
// ========================================================================

TEST_F(AssetRegistryTest, HandlePathOnNullReturnsEmpty) {
    Handle<TestAsset> h;
    EXPECT_TRUE(h.path().empty());
}

TEST_F(AssetRegistryTest, HandleErrorOnLoadedReturnsEmpty) {
    AssetRegistry registry(hub);
    registry.registerLoader<TestAsset>(std::make_unique<TestAssetLoader>());

    auto path = writeTempFile("no_err.test", "data");
    auto handle = registry.load<TestAsset>(path);

    EXPECT_TRUE(handle.error().empty());
}

// ========================================================================
// Audit: Loader returning nullptr transitions to Failed
// ========================================================================

TEST_F(AssetRegistryTest, LoaderReturningNullptrSetsFailedState) {
    struct NullLoader : AssetLoader<TestAsset> {
        std::unique_ptr<TestAsset> load(const std::filesystem::path&, ResourceHub&) override { return nullptr; }
        std::vector<std::string> extensions() const override { return {".test"}; }
    };

    AssetRegistry registry(hub);
    registry.registerLoader<TestAsset>(std::make_unique<NullLoader>());

    auto path = writeTempFile("null_ret.test", "data");
    auto handle = registry.load<TestAsset>(path);

    // Loader returned nullptr: state must be Failed, not Loaded
    EXPECT_EQ(handle.state(), AssetState::Failed);
    EXPECT_FALSE(handle.isLoaded());
    EXPECT_TRUE(handle.isFailed());
    EXPECT_FALSE(handle.error().empty());
    EXPECT_EQ(handle.tryGet(), nullptr);
    EXPECT_THROW(handle.get(), std::exception);
}

// ========================================================================
// Audit: AssetRegistry type-erasure edge cases
// ========================================================================

TEST_F(AssetRegistryTest, GetUnregisteredTypeReturnsNull) {
    AssetRegistry registry(hub);
    auto h = registry.get<TestAsset>("/some/path.test");
    EXPECT_FALSE(static_cast<bool>(h));
}

TEST_F(AssetRegistryTest, CountUnregisteredTypeReturnsZero) {
    AssetRegistry registry(hub);
    EXPECT_EQ(registry.count<TestAsset>(), 0u);
}

TEST_F(AssetRegistryTest, MemoryUsageUnregisteredTypeReturnsZero) {
    AssetRegistry registry(hub);
    EXPECT_EQ(registry.getMemoryUsage<TestAsset>(), 0u);
}

TEST_F(AssetRegistryTest, TotalCountOnEmptyRegistryIsZero) {
    AssetRegistry registry(hub);
    EXPECT_EQ(registry.totalCount(), 0u);
}

// ========================================================================
// Audit: Path normalization deduplication
// ========================================================================

TEST_F(AssetRegistryTest, PathNormalizationDeduplicates) {
    AssetRegistry registry(hub);
    registry.registerLoader<TestAsset>(std::make_unique<TestAssetLoader>());

    auto path = writeTempFile("norm.test", "data");
    auto h1 = registry.load<TestAsset>(path);

    // Construct an equivalent path with .. that resolves to the same file
    auto altPath = tempDir / ".." / tempDir.filename() / "norm.test";
    auto h2 = registry.load<TestAsset>(altPath);

    EXPECT_EQ(h1, h2);
    EXPECT_EQ(registry.count<TestAsset>(), 1u);
}

// ========================================================================
// Audit: LRU eviction invariants
// ========================================================================

TEST_F(AssetRegistryTest, LRUNoEvictionWhenAtExactBudget) {
    AssetRegistry registry(hub);
    registry.registerLoader<TestAsset>(std::make_unique<TestAssetLoader>());

    auto p1 = writeTempFile("exact1.test", "a");
    auto p2 = writeTempFile("exact2.test", "b");
    registry.load<TestAsset>(p1);
    registry.load<TestAsset>(p2);

    // Budget exactly fits 2 assets: no eviction should occur
    registry.setMemoryBudget<TestAsset>(sizeof(TestAsset) * 2);
    registry.update();

    EXPECT_EQ(registry.count<TestAsset>(), 2u);
}

TEST_F(AssetRegistryTest, LRUAccessViaGetRefreshesPosition) {
    AssetRegistry registry(hub);
    registry.registerLoader<TestAsset>(std::make_unique<TestAssetLoader>());

    auto p1 = writeTempFile("lru_r1.test", "first");
    auto p2 = writeTempFile("lru_r2.test", "second");
    auto p3 = writeTempFile("lru_r3.test", "third");

    registry.load<TestAsset>(p1); // tick 0
    registry.update();            // tick 1
    registry.load<TestAsset>(p2); // tick 1
    registry.update();            // tick 2
    registry.load<TestAsset>(p3); // tick 2
    registry.update();            // tick 3

    // Refresh p1 via get() so it is no longer the oldest
    auto refreshed = registry.get<TestAsset>(p1);
    EXPECT_TRUE(static_cast<bool>(refreshed));

    // Budget for 2 assets. p2 should be evicted (oldest access), not p1.
    registry.setMemoryBudget<TestAsset>(sizeof(TestAsset) * 2);
    registry.update(); // tick 4, triggers eviction

    auto h1 = registry.get<TestAsset>(p1);
    auto h2 = registry.get<TestAsset>(p2);
    auto h3 = registry.get<TestAsset>(p3);

    EXPECT_TRUE(static_cast<bool>(h1));
    EXPECT_FALSE(static_cast<bool>(h2));
    EXPECT_TRUE(static_cast<bool>(h3));
}

TEST_F(AssetRegistryTest, LRUEvictsMultipleToFitBudget) {
    AssetRegistry registry(hub);
    registry.registerLoader<TestAsset>(std::make_unique<TestAssetLoader>());

    for (int i = 0; i < 5; ++i) {
        auto p = writeTempFile("mevict" + std::to_string(i) + ".test", "d");
        registry.load<TestAsset>(p);
        registry.update();
    }
    EXPECT_EQ(registry.count<TestAsset>(), 5u);

    // Budget for only 1 asset: 4 must be evicted
    registry.setMemoryBudget<TestAsset>(sizeof(TestAsset));
    registry.update();

    EXPECT_LE(registry.count<TestAsset>(), 1u);
}

// ========================================================================
// Audit: Per-type memory budgets
// ========================================================================

TEST_F(AssetRegistryTest, NoBudgetMeansNoEviction) {
    AssetRegistry registry(hub);
    registry.registerLoader<TestAsset>(std::make_unique<TestAssetLoader>());

    for (int i = 0; i < 10; ++i) {
        auto p = writeTempFile("nobud" + std::to_string(i) + ".test", "data");
        registry.load<TestAsset>(p);
        registry.update();
    }

    // Default budget is 0 which means unlimited: all 10 survive
    EXPECT_EQ(registry.count<TestAsset>(), 10u);
}

TEST_F(AssetRegistryTest, IndependentBudgetsPerType) {
    AssetRegistry registry(hub);
    registry.registerLoader<TestAsset>(std::make_unique<TestAssetLoader>());
    registry.registerLoader<OtherAsset>(std::make_unique<OtherAssetLoader>());

    auto p1 = writeTempFile("ind1.test", "a");
    auto p2 = writeTempFile("ind2.test", "b");
    auto p3 = writeTempFile("ind3.test", "c");
    registry.load<TestAsset>(p1);
    registry.update();
    registry.load<TestAsset>(p2);
    registry.update();
    registry.load<TestAsset>(p3);

    auto o1 = writeTempFile("ind1.other", "x");
    auto o2 = writeTempFile("ind2.other", "y");
    registry.load<OtherAsset>(o1);
    registry.update();
    registry.load<OtherAsset>(o2);

    // Tight budget for TestAsset only; OtherAsset has no budget
    registry.setMemoryBudget<TestAsset>(sizeof(TestAsset));
    registry.update();

    EXPECT_LE(registry.count<TestAsset>(), 1u);
    EXPECT_EQ(registry.count<OtherAsset>(), 2u);
}

// ========================================================================
// Audit: Stress patterns
// ========================================================================

TEST_F(AssetRegistryTest, RapidLoadUnloadCycles) {
    AssetRegistry registry(hub);
    registry.registerLoader<TestAsset>(std::make_unique<TestAssetLoader>());
    auto path = writeTempFile("rapid.test", "cycle");

    for (int i = 0; i < 20; ++i) {
        auto handle = registry.load<TestAsset>(path);
        EXPECT_TRUE(handle.isLoaded());
        registry.unload<TestAsset>(path);
        EXPECT_EQ(registry.count<TestAsset>(), 0u);
    }
}

TEST_F(AssetRegistryTest, ManyHandlesToSameAsset) {
    AssetRegistry registry(hub);
    registry.registerLoader<TestAsset>(std::make_unique<TestAssetLoader>());
    auto path = writeTempFile("shared.test", "shared_data");

    std::vector<Handle<TestAsset>> handles;
    for (int i = 0; i < 50; ++i) {
        handles.push_back(registry.load<TestAsset>(path));
    }

    // All handles point to the same underlying asset
    for (size_t i = 1; i < handles.size(); ++i) {
        EXPECT_EQ(&handles[i].get(), &handles[0].get());
    }
    EXPECT_EQ(registry.count<TestAsset>(), 1u);
}

// ========================================================================
// Audit: Reload edge cases
// ========================================================================

TEST_F(AssetRegistryTest, ReloadNonExistentPathIsNoop) {
    AssetRegistry registry(hub);
    registry.registerLoader<TestAsset>(std::make_unique<TestAssetLoader>());
    EXPECT_NO_THROW(registry.reload<TestAsset>("/nonexistent/path.test"));
}

TEST_F(AssetRegistryTest, ReloadUnregisteredTypeIsNoop) {
    AssetRegistry registry(hub);
    EXPECT_NO_THROW(registry.reload<TestAsset>("/some/path.test"));
}

// ========================================================================
// Audit: Unload edge cases
// ========================================================================

TEST_F(AssetRegistryTest, UnloadNullHandleIsNoop) {
    AssetRegistry registry(hub);
    registry.registerLoader<TestAsset>(std::make_unique<TestAssetLoader>());
    Handle<TestAsset> h;
    EXPECT_NO_THROW(registry.unload(h));
}

TEST_F(AssetRegistryTest, DoubleUnloadIsNoop) {
    AssetRegistry registry(hub);
    registry.registerLoader<TestAsset>(std::make_unique<TestAssetLoader>());
    auto path = writeTempFile("dbl_unload.test", "data");
    registry.load<TestAsset>(path);
    registry.unload<TestAsset>(path);
    EXPECT_NO_THROW(registry.unload<TestAsset>(path));
}

TEST_F(AssetRegistryTest, HandleStateAfterUnload) {
    AssetRegistry registry(hub);
    registry.registerLoader<TestAsset>(std::make_unique<TestAssetLoader>());
    auto path = writeTempFile("after_unload.test", "data");
    auto handle = registry.load<TestAsset>(path);
    EXPECT_TRUE(handle.isLoaded());

    registry.unload<TestAsset>(path);

    // Handle still exists (shared_ptr alive) but asset is gone
    EXPECT_EQ(handle.state(), AssetState::Unloaded);
    EXPECT_EQ(handle.tryGet(), nullptr);
    EXPECT_FALSE(handle.isLoaded());
    EXPECT_THROW(handle.get(), std::exception);
    // The handle itself is still non-null (block_ shared_ptr is valid)
    EXPECT_TRUE(static_cast<bool>(handle));
}

// ========================================================================
// Bug fix: LRU eviction skips in-use handles (ref count protection)
// ========================================================================

TEST_F(AssetRegistryTest, LRUEvictionSkipsInUseHandles) {
    AssetRegistry registry(hub);
    registry.registerLoader<TestAsset>(std::make_unique<TestAssetLoader>());

    auto pA = writeTempFile("lru_inuse_a.test", "A");
    auto pB = writeTempFile("lru_inuse_b.test", "B");
    auto pC = writeTempFile("lru_inuse_c.test", "C");

    // Load A first (oldest), keep a handle alive
    auto handleA = registry.load<TestAsset>(pA); // tick 0
    registry.update();                           // tick 1

    // Load B and C, let their handles go out of scope
    {
        registry.load<TestAsset>(pB); // tick 1
        registry.update();            // tick 2
        registry.load<TestAsset>(pC); // tick 2
    }
    EXPECT_EQ(registry.count<TestAsset>(), 3u);

    // Budget for 2: must evict 1. A is oldest but in-use (handleA holds ref).
    registry.setMemoryBudget<TestAsset>(sizeof(TestAsset) * 2);
    registry.update(); // tick 3, eviction runs

    // A must survive because handleA keeps use_count > 1
    EXPECT_TRUE(handleA.isLoaded());
    EXPECT_EQ(handleA.get().data, "A");
    EXPECT_EQ(registry.count<TestAsset>(), 2u);

    // A must still be in the cache
    auto hA = registry.get<TestAsset>(pA);
    EXPECT_TRUE(static_cast<bool>(hA));
}

} // namespace fabric
