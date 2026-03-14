#include "recurse/persistence/ChunkSaveService.hh"

#include "fabric/platform/WriterQueue.hh"
#include "recurse/persistence/FchkCodec.hh"
#include "recurse/persistence/SqliteChunkStore.hh"
#include <filesystem>
#include <gtest/gtest.h>

namespace fs = std::filesystem;

class ChunkSaveServiceTest : public ::testing::Test {
  protected:
    void SetUp() override {
        tmpDir_ = fs::temp_directory_path() / "fabric_test_save_service";
        fs::remove_all(tmpDir_);
        worldDir_ = (tmpDir_ / "testworld").string();
        store_ = std::make_unique<recurse::SqliteChunkStore>(worldDir_);
    }
    void TearDown() override { fs::remove_all(tmpDir_); }

    fs::path tmpDir_;
    std::string worldDir_;
    std::unique_ptr<recurse::SqliteChunkStore> store_;
    fabric::platform::WriterQueue writerQueue_;

    static recurse::ChunkBlob makeFakeBlob(uint8_t marker = 0xAA) {
        constexpr size_t payloadSize = 32 * 32 * 32 * 4;
        std::vector<uint8_t> cells(payloadSize, 0);
        cells[0] = marker;
        return recurse::FchkCodec::encode(cells.data(), cells.size());
    }
};

TEST_F(ChunkSaveServiceTest, MarkDirtyIncreasesPendingCount) {
    recurse::ChunkSaveService svc(*store_, writerQueue_, [&](int, int, int) { return makeFakeBlob(); });

    EXPECT_EQ(svc.pendingCount(), 0u);
    svc.markDirty(0, 0, 0);
    EXPECT_EQ(svc.pendingCount(), 1u);
    svc.markDirty(1, 0, 0);
    EXPECT_EQ(svc.pendingCount(), 2u);
}

TEST_F(ChunkSaveServiceTest, FlushSavesAllDirtyChunks) {
    recurse::ChunkSaveService svc(*store_, writerQueue_, [&](int, int, int) { return makeFakeBlob(); });

    svc.markDirty(0, 0, 0);
    svc.markDirty(1, 2, 3);
    svc.flush();

    EXPECT_EQ(svc.pendingCount(), 0u);
    EXPECT_TRUE(store_->hasChunk(0, 0, 0));
    EXPECT_TRUE(store_->hasChunk(1, 2, 3));
}

TEST_F(ChunkSaveServiceTest, DebounceDelaysSave) {
    recurse::ChunkSaveService svc(*store_, writerQueue_, [&](int, int, int) { return makeFakeBlob(); });
    svc.debounceSeconds = 1.0f;
    svc.maxDelaySeconds = 5.0f;

    svc.markDirty(0, 0, 0);

    svc.update(0.5f);
    EXPECT_FALSE(store_->hasChunk(0, 0, 0));

    svc.update(0.6f);
    writerQueue_.drain();
    EXPECT_TRUE(store_->hasChunk(0, 0, 0));
}

TEST_F(ChunkSaveServiceTest, MaxDelayForcesSave) {
    recurse::ChunkSaveService svc(*store_, writerQueue_, [&](int, int, int) { return makeFakeBlob(); });
    svc.debounceSeconds = 2.0f;
    svc.maxDelaySeconds = 3.0f;

    svc.markDirty(0, 0, 0);

    svc.update(1.0f);
    svc.markDirty(0, 0, 0);
    svc.update(1.0f);
    svc.markDirty(0, 0, 0);
    EXPECT_FALSE(store_->hasChunk(0, 0, 0));

    svc.update(1.1f);
    writerQueue_.drain();
    EXPECT_TRUE(store_->hasChunk(0, 0, 0));
}

TEST_F(ChunkSaveServiceTest, SecondSaveOverwrites) {
    recurse::ChunkSaveService svc(*store_, writerQueue_, [&](int, int, int) { return makeFakeBlob(); });

    svc.markDirty(0, 0, 0);
    svc.flush();
    EXPECT_TRUE(store_->hasChunk(0, 0, 0));

    svc.markDirty(0, 0, 0);
    svc.flush();
    EXPECT_TRUE(store_->hasChunk(0, 0, 0));
}

TEST_F(ChunkSaveServiceTest, NegativeCoordinates) {
    recurse::ChunkSaveService svc(*store_, writerQueue_, [&](int, int, int) { return makeFakeBlob(); });

    svc.markDirty(-3, -7, 5);
    svc.flush();
    EXPECT_TRUE(store_->hasChunk(-3, -7, 5));
}

TEST_F(ChunkSaveServiceTest, EmptyBlobSkipsSave) {
    recurse::ChunkSaveService svc(*store_, writerQueue_, [&](int, int, int) { return recurse::ChunkBlob{}; });

    svc.markDirty(0, 0, 0);
    svc.flush();
    EXPECT_FALSE(store_->hasChunk(0, 0, 0));
}
