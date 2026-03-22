#include "recurse/persistence/ChunkSaveService.hh"

#include "fabric/platform/WriterQueue.hh"
#include "recurse/persistence/ChunkStore.hh"
#include "recurse/persistence/FchkCodec.hh"
#include "recurse/persistence/SqliteChunkStore.hh"
#include <atomic>
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

TEST_F(ChunkSaveServiceTest, ActivitySnapshotResetsCountdownForNewDirtyChunk) {
    recurse::ChunkSaveService svc(*store_, writerQueue_, [&](int, int, int) { return makeFakeBlob(); });
    svc.debounceSeconds = 2.0f;
    svc.maxDelaySeconds = 5.0f;

    svc.markDirty(0, 0, 0);
    svc.update(1.25f);

    auto snapshot = svc.activitySnapshot();
    EXPECT_NEAR(snapshot.secondsUntilNextSave, 0.75f, 0.001f);

    svc.markDirty(1, 0, 0);
    snapshot = svc.activitySnapshot();
    EXPECT_FLOAT_EQ(snapshot.secondsUntilNextSave, 2.0f);
}

TEST_F(ChunkSaveServiceTest, TimerAutosaveMetadataStaysHiddenForShortDebouncedEdits) {
    recurse::ChunkSaveService svc(*store_, writerQueue_, [&](int, int, int) { return makeFakeBlob(); });
    svc.debounceSeconds = 2.0f;
    svc.maxDelaySeconds = 5.0f;

    svc.markDirty(0, 0, 0);

    auto snapshot = svc.activitySnapshot();
    EXPECT_FALSE(snapshot.timerAutosavePending);
    EXPECT_FALSE(snapshot.timerSaveInProgress);
    EXPECT_FLOAT_EQ(snapshot.secondsUntilTimerAutosave, -1.0f);

    svc.update(1.0f);
    snapshot = svc.activitySnapshot();
    EXPECT_FALSE(snapshot.timerAutosavePending);
    EXPECT_FALSE(snapshot.timerSaveInProgress);
    EXPECT_FLOAT_EQ(snapshot.secondsUntilTimerAutosave, -1.0f);

    svc.update(1.1f);
    writerQueue_.drain();
    snapshot = svc.activitySnapshot();
    EXPECT_EQ(snapshot.lastTimerStartedSerial, 0u);
    EXPECT_EQ(snapshot.lastTimerSuccessfulSerial, 0u);
}

TEST_F(ChunkSaveServiceTest, TimerAutosaveMetadataTracksForcedAutosaveSeparately) {
    std::promise<void> unblockSave;
    auto gate = unblockSave.get_future().share();

    recurse::ChunkSaveService svc(*store_, writerQueue_, [&](int, int, int) {
        gate.wait();
        return makeFakeBlob();
    });
    svc.debounceSeconds = 2.0f;
    svc.maxDelaySeconds = 3.0f;

    svc.markDirty(0, 0, 0);
    svc.update(1.5f);
    svc.markDirty(0, 0, 0);

    auto snapshot = svc.activitySnapshot();
    EXPECT_FALSE(snapshot.timerAutosavePending);

    svc.update(0.6f);
    snapshot = svc.activitySnapshot();
    EXPECT_TRUE(snapshot.timerAutosavePending);
    EXPECT_NEAR(snapshot.secondsUntilTimerAutosave, 0.9f, 0.001f);
    EXPECT_FALSE(snapshot.timerSaveInProgress);

    svc.markDirty(0, 0, 0);
    svc.update(1.1f);
    snapshot = svc.activitySnapshot();
    EXPECT_EQ(snapshot.lastTimerStartedSerial, 1u);
    EXPECT_TRUE(snapshot.timerSaveInProgress);

    unblockSave.set_value();
    writerQueue_.drain();

    snapshot = svc.activitySnapshot();
    EXPECT_FALSE(snapshot.timerAutosavePending);
    EXPECT_FALSE(snapshot.timerSaveInProgress);
    EXPECT_EQ(snapshot.lastTimerSuccessfulSerial, 1u);
    EXPECT_TRUE(store_->hasChunk(0, 0, 0));
}

TEST_F(ChunkSaveServiceTest, DebounceCoalescesDirtyChunksIntoOneBatchCadence) {
    recurse::ChunkSaveService svc(*store_, writerQueue_, [&](int, int, int) { return makeFakeBlob(); });
    svc.debounceSeconds = 1.0f;
    svc.maxDelaySeconds = 5.0f;

    svc.markDirty(0, 0, 0);
    svc.update(0.75f);
    svc.markDirty(1, 0, 0);

    svc.update(0.3f);
    writerQueue_.drain();
    EXPECT_FALSE(store_->hasChunk(0, 0, 0));
    EXPECT_FALSE(store_->hasChunk(1, 0, 0));

    auto snapshot = svc.activitySnapshot();
    EXPECT_EQ(snapshot.lastStartedSerial, 0u);
    EXPECT_NEAR(snapshot.secondsUntilNextSave, 0.7f, 0.001f);

    svc.update(0.7f);
    writerQueue_.drain();

    EXPECT_TRUE(store_->hasChunk(0, 0, 0));
    EXPECT_TRUE(store_->hasChunk(1, 0, 0));

    snapshot = svc.activitySnapshot();
    EXPECT_EQ(snapshot.lastSuccessfulSerial, 1u);
}

TEST_F(ChunkSaveServiceTest, PreparedVersionIncrementsOnOverwrite) {
    recurse::ChunkSaveService svc(*store_, writerQueue_, [&](int, int, int) { return makeFakeBlob(); });

    svc.enqueuePrepared(1, 2, 3, makeFakeBlob(0x11));
    auto v1 = svc.copyPersistPendingBlob(1, 2, 3);
    ASSERT_TRUE(v1.has_value());
    EXPECT_GT(v1->version, 0u);

    svc.enqueuePrepared(1, 2, 3, makeFakeBlob(0x22));
    auto v2 = svc.copyPersistPendingBlob(1, 2, 3);
    ASSERT_TRUE(v2.has_value());
    EXPECT_GT(v2->version, v1->version);

    svc.enqueuePrepared(4, 5, 6, makeFakeBlob(0x33));
    auto v3 = svc.copyPersistPendingBlob(4, 5, 6);
    ASSERT_TRUE(v3.has_value());
    EXPECT_GT(v3->version, v2->version);
}

TEST_F(ChunkSaveServiceTest, CopyPersistPendingBlobReturnsVersionedBlob) {
    recurse::ChunkSaveService svc(*store_, writerQueue_, [&](int, int, int) { return makeFakeBlob(); });

    auto empty = svc.copyPersistPendingBlob(0, 0, 0);
    EXPECT_FALSE(empty.has_value());

    svc.enqueuePrepared(0, 0, 0, makeFakeBlob(0x42));
    auto result = svc.copyPersistPendingBlob(0, 0, 0);
    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result->blob.empty());
    EXPECT_GT(result->version, 0u);
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

// ---------------------------------------------------------------------------
// Throwing store for retry tests
// ---------------------------------------------------------------------------

namespace {

class ThrowingChunkStore : public recurse::ChunkStore {
  public:
    explicit ThrowingChunkStore(recurse::ChunkStore& delegate) : delegate_(delegate) {}

    std::atomic<int> failCount{0}; // saveBatch throws until failCount reaches 0

    bool hasChunk(int cx, int cy, int cz) const override { return delegate_.hasChunk(cx, cy, cz); }
    std::optional<recurse::ChunkBlob> loadChunk(int cx, int cy, int cz) const override {
        return delegate_.loadChunk(cx, cy, cz);
    }
    void saveChunk(int cx, int cy, int cz, const recurse::ChunkBlob& data) override {
        delegate_.saveChunk(cx, cy, cz, data);
    }
    size_t chunkSize(int cx, int cy, int cz) const override { return delegate_.chunkSize(cx, cy, cz); }
    std::vector<std::pair<fabric::ChunkCoord, recurse::ChunkBlob>>
    loadBatch(const std::vector<fabric::ChunkCoord>& coords) const override {
        return delegate_.loadBatch(coords);
    }
    void saveBatch(const std::vector<std::pair<fabric::ChunkCoord, recurse::ChunkBlob>>& entries) override {
        int prev = failCount.fetch_sub(1);
        if (prev > 0)
            throw std::runtime_error("transient disk error");
        delegate_.saveBatch(entries);
    }

  private:
    recurse::ChunkStore& delegate_;
};

} // namespace

// ---------------------------------------------------------------------------
// Retry tests
// ---------------------------------------------------------------------------

TEST_F(ChunkSaveServiceTest, FlushRetriesOnTransientFailure) {
    ThrowingChunkStore throwing(*store_);
    recurse::ChunkSaveService svc(throwing, writerQueue_, [&](int, int, int) { return makeFakeBlob(); });

    svc.markDirty(0, 0, 0);

    // First saveBatch call throws, retry succeeds
    throwing.failCount = 1;
    EXPECT_NO_THROW(svc.flush());

    EXPECT_EQ(svc.pendingCount(), 0u);
    EXPECT_TRUE(store_->hasChunk(0, 0, 0));
}

TEST_F(ChunkSaveServiceTest, FlushThrowsAfterRetryExhausted) {
    ThrowingChunkStore throwing(*store_);
    recurse::ChunkSaveService svc(throwing, writerQueue_, [&](int, int, int) { return makeFakeBlob(); });

    svc.markDirty(0, 0, 0);

    // Both attempts fail (2 failures = first + retry both throw)
    throwing.failCount = 2;
    EXPECT_THROW(svc.flush(), std::runtime_error);

    // Data still pending since both attempts failed
    EXPECT_GT(svc.pendingCount(), 0u);
    EXPECT_FALSE(store_->hasChunk(0, 0, 0));

    auto snapshot = svc.activitySnapshot();
    EXPECT_TRUE(snapshot.hasError);
}

TEST_F(ChunkSaveServiceTest, FlushRetryAlsoWorksPreparedEntries) {
    ThrowingChunkStore throwing(*store_);
    recurse::ChunkSaveService svc(throwing, writerQueue_, [&](int, int, int) { return makeFakeBlob(); });

    svc.enqueuePrepared(2, 3, 4, makeFakeBlob(0xBB));

    // First attempt fails, retry succeeds
    throwing.failCount = 1;
    EXPECT_NO_THROW(svc.flush());

    EXPECT_EQ(svc.pendingCount(), 0u);
    EXPECT_TRUE(store_->hasChunk(2, 3, 4));
}
