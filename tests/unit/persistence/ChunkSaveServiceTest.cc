#include "recurse/persistence/ChunkSaveService.hh"

#include "fabric/platform/WriterQueue.hh"
#include "recurse/persistence/FchkCodec.hh"
#include "recurse/persistence/SqliteChunkStore.hh"
#include <filesystem>
#include <future>
#include <gtest/gtest.h>
#include <stdexcept>

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

TEST_F(ChunkSaveServiceTest, PreparedOnlyUpdatePersistsQueuedBlob) {
    recurse::ChunkSaveService svc(*store_, writerQueue_, [&](int, int, int) { return makeFakeBlob(); });

    svc.enqueuePrepared(4, 5, 6, makeFakeBlob(0x11));
    EXPECT_EQ(svc.pendingCount(), 1u);
    EXPECT_TRUE(svc.hasPersistPending(4, 5, 6));

    svc.update(0.0f);
    writerQueue_.drain();

    auto snapshot = svc.activitySnapshot();
    EXPECT_EQ(snapshot.preparedChunks, 0u);
    EXPECT_EQ(svc.pendingCount(), 0u);
    EXPECT_FALSE(svc.hasPersistPending(4, 5, 6));
    EXPECT_TRUE(store_->hasChunk(4, 5, 6));
}

TEST_F(ChunkSaveServiceTest, EnqueuePreparedDedupesSameCoord) {
    recurse::ChunkSaveService svc(*store_, writerQueue_, [&](int, int, int) { return makeFakeBlob(); });

    svc.enqueuePrepared(1, 2, 3, makeFakeBlob(0x11));
    svc.enqueuePrepared(1, 2, 3, makeFakeBlob(0x22));

    auto snapshot = svc.activitySnapshot();
    EXPECT_EQ(snapshot.preparedChunks, 1u);
    EXPECT_EQ(svc.pendingCount(), 1u);

    svc.update(0.0f);
    writerQueue_.drain();

    auto loaded = store_->loadChunk(1, 2, 3);
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->data, makeFakeBlob(0x22).data);
}

TEST_F(ChunkSaveServiceTest, ActivitySnapshotTracksDirtySavingAndSuccess) {
    std::promise<void> unblockSave;
    auto gate = unblockSave.get_future().share();

    recurse::ChunkSaveService svc(*store_, writerQueue_, [&](int, int, int) {
        gate.wait();
        return makeFakeBlob();
    });
    svc.debounceSeconds = 0.0f;
    svc.maxDelaySeconds = 0.0f;

    svc.markDirty(0, 0, 0);
    auto snapshot = svc.activitySnapshot();
    EXPECT_EQ(snapshot.dirtyChunks, 1u);
    EXPECT_EQ(snapshot.savingChunks, 0u);
    EXPECT_FLOAT_EQ(snapshot.secondsUntilNextSave, 0.0f);
    EXPECT_EQ(snapshot.lastStartedSerial, 0u);

    svc.update(0.0f);
    snapshot = svc.activitySnapshot();
    EXPECT_EQ(snapshot.dirtyChunks, 1u);
    EXPECT_EQ(snapshot.savingChunks, 1u);
    EXPECT_EQ(snapshot.lastStartedSerial, 1u);
    EXPECT_EQ(snapshot.lastSuccessfulSerial, 0u);

    unblockSave.set_value();
    writerQueue_.drain();

    snapshot = svc.activitySnapshot();
    EXPECT_EQ(snapshot.dirtyChunks, 0u);
    EXPECT_EQ(snapshot.savingChunks, 0u);
    EXPECT_EQ(snapshot.lastCompletedSerial, 1u);
    EXPECT_EQ(snapshot.lastSuccessfulSerial, 1u);
    EXPECT_FALSE(snapshot.hasError);
    EXPECT_TRUE(store_->hasChunk(0, 0, 0));
}

TEST_F(ChunkSaveServiceTest, ActivitySnapshotTracksNextAutosaveCountdown) {
    recurse::ChunkSaveService svc(*store_, writerQueue_, [&](int, int, int) { return makeFakeBlob(); });
    svc.debounceSeconds = 2.0f;
    svc.maxDelaySeconds = 5.0f;

    svc.markDirty(0, 0, 0);
    auto snapshot = svc.activitySnapshot();
    EXPECT_FLOAT_EQ(snapshot.secondsUntilNextSave, 2.0f);

    svc.update(0.75f);
    snapshot = svc.activitySnapshot();
    EXPECT_NEAR(snapshot.secondsUntilNextSave, 1.25f, 0.001f);

    svc.markDirty(0, 0, 0);
    snapshot = svc.activitySnapshot();
    EXPECT_FLOAT_EQ(snapshot.secondsUntilNextSave, 2.0f);
}

TEST_F(ChunkSaveServiceTest, ActivitySnapshotRetainsFailureAndRequeuesPreparedBlobs) {
    recurse::ChunkSaveService svc(
        *store_, writerQueue_, [&](int, int, int) -> recurse::ChunkBlob { throw std::runtime_error("save failed"); });
    svc.debounceSeconds = 0.0f;
    svc.maxDelaySeconds = 0.0f;

    svc.markDirty(0, 0, 0);
    svc.enqueuePrepared(1, 2, 3, makeFakeBlob(0x33));

    auto snapshot = svc.activitySnapshot();
    EXPECT_EQ(snapshot.preparedChunks, 1u);

    svc.update(0.0f);
    writerQueue_.drain();

    snapshot = svc.activitySnapshot();
    EXPECT_EQ(snapshot.dirtyChunks, 1u);
    EXPECT_EQ(snapshot.savingChunks, 0u);
    EXPECT_EQ(snapshot.preparedChunks, 1u);
    EXPECT_EQ(snapshot.lastCompletedSerial, 1u);
    EXPECT_EQ(snapshot.lastSuccessfulSerial, 0u);
    EXPECT_TRUE(snapshot.hasError);
    EXPECT_EQ(snapshot.lastError, "save failed");
    EXPECT_FALSE(store_->hasChunk(1, 2, 3));
}
