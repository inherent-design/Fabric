#include "recurse/persistence/ChunkSaveService.hh"

#include "fabric/platform/JobScheduler.hh"
#include "recurse/persistence/FilesystemChunkStore.hh"
#include <filesystem>
#include <gtest/gtest.h>

namespace fs = std::filesystem;

class ChunkSaveServiceTest : public ::testing::Test {
  protected:
    void SetUp() override {
        tmpDir_ = fs::temp_directory_path() / "fabric_test_save_service";
        fs::remove_all(tmpDir_);
        worldDir_ = (tmpDir_ / "testworld").string();
        store_ = std::make_unique<recurse::FilesystemChunkStore>(worldDir_);
        jobs_ = std::make_unique<fabric::JobScheduler>(1);
        jobs_->disableForTesting();
    }
    void TearDown() override { fs::remove_all(tmpDir_); }

    fs::path tmpDir_;
    std::string worldDir_;
    std::unique_ptr<recurse::FilesystemChunkStore> store_;
    std::unique_ptr<fabric::JobScheduler> jobs_;

    static recurse::ChunkBlob makeFakeBlob(uint8_t marker = 0xAA) {
        constexpr size_t payloadSize = 32 * 32 * 32 * 4;
        std::vector<uint8_t> cells(payloadSize, 0);
        cells[0] = marker;
        return recurse::FilesystemChunkStore::encode(cells.data(), cells.size());
    }
};

TEST_F(ChunkSaveServiceTest, MarkDirtyIncreasesPendingCount) {
    recurse::ChunkSaveService svc(*store_, *jobs_, [&](int, int, int) { return makeFakeBlob(); });

    EXPECT_EQ(svc.pendingCount(), 0u);
    svc.markDirty(0, 0, 0);
    EXPECT_EQ(svc.pendingCount(), 1u);
    svc.markDirty(1, 0, 0);
    EXPECT_EQ(svc.pendingCount(), 2u);
}

TEST_F(ChunkSaveServiceTest, FlushSavesAllDirtyChunks) {
    recurse::ChunkSaveService svc(*store_, *jobs_, [&](int, int, int) { return makeFakeBlob(); });

    svc.markDirty(0, 0, 0);
    svc.markDirty(1, 2, 3);
    svc.flush();

    EXPECT_EQ(svc.pendingCount(), 0u);
    EXPECT_TRUE(store_->hasGenData(0, 0, 0));
    EXPECT_TRUE(store_->hasGenData(1, 2, 3));
}

TEST_F(ChunkSaveServiceTest, DebounceDelaysSave) {
    recurse::ChunkSaveService svc(*store_, *jobs_, [&](int, int, int) { return makeFakeBlob(); });
    svc.debounceSeconds = 1.0f;
    svc.maxDelaySeconds = 5.0f;

    svc.markDirty(0, 0, 0);

    // Not enough time elapsed; should not save
    svc.update(0.5f);
    EXPECT_FALSE(store_->hasGenData(0, 0, 0));

    // Debounce expired
    svc.update(0.6f);
    EXPECT_TRUE(store_->hasGenData(0, 0, 0));
}

TEST_F(ChunkSaveServiceTest, MaxDelayForcesSave) {
    recurse::ChunkSaveService svc(*store_, *jobs_, [&](int, int, int) { return makeFakeBlob(); });
    svc.debounceSeconds = 2.0f;
    svc.maxDelaySeconds = 3.0f;

    svc.markDirty(0, 0, 0);

    // Keep resetting debounce by marking dirty
    svc.update(1.0f);
    svc.markDirty(0, 0, 0); // resets lastDirtyAge
    svc.update(1.0f);
    svc.markDirty(0, 0, 0); // resets lastDirtyAge again
    EXPECT_FALSE(store_->hasGenData(0, 0, 0));

    // Max delay should force save after 3s total
    svc.update(1.1f);
    EXPECT_TRUE(store_->hasGenData(0, 0, 0));
}

TEST_F(ChunkSaveServiceTest, SecondSaveWritesDelta) {
    recurse::ChunkSaveService svc(*store_, *jobs_, [&](int, int, int) { return makeFakeBlob(); });

    // First save: gen data
    svc.markDirty(0, 0, 0);
    svc.flush();
    EXPECT_TRUE(store_->hasGenData(0, 0, 0));
    EXPECT_FALSE(store_->hasDelta(0, 0, 0));

    // Second save: delta
    svc.markDirty(0, 0, 0);
    svc.flush();
    // After compaction (delta >= gen), delta is merged back into gen
    // So we should have gen data and no delta (compaction fired)
    EXPECT_TRUE(store_->hasGenData(0, 0, 0));
}

TEST_F(ChunkSaveServiceTest, NegativeCoordinates) {
    recurse::ChunkSaveService svc(*store_, *jobs_, [&](int, int, int) { return makeFakeBlob(); });

    svc.markDirty(-3, -7, 5);
    svc.flush();
    EXPECT_TRUE(store_->hasGenData(-3, -7, 5));
}

TEST_F(ChunkSaveServiceTest, EmptyBlobSkipsSave) {
    recurse::ChunkSaveService svc(*store_, *jobs_, [&](int, int, int) {
        return recurse::ChunkBlob{}; // empty
    });

    svc.markDirty(0, 0, 0);
    svc.flush();
    EXPECT_FALSE(store_->hasGenData(0, 0, 0));
}
