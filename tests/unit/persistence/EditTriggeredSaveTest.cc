#include "fabric/core/Event.hh"
#include "fabric/platform/WriterQueue.hh"
#include "recurse/character/VoxelInteraction.hh"
#include "recurse/persistence/ChunkSaveService.hh"
#include "recurse/persistence/FchkCodec.hh"
#include "recurse/persistence/SqliteChunkStore.hh"
#include <filesystem>
#include <gtest/gtest.h>

namespace fs = std::filesystem;

class EditTriggeredSaveTest : public ::testing::Test {
  protected:
    void SetUp() override {
        tmpDir_ = fs::temp_directory_path() / "fabric_test_edit_save";
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

TEST_F(EditTriggeredSaveTest, EventTriggersMarkDirty) {
    recurse::ChunkSaveService svc(*store_, writerQueue_, [&](int, int, int) { return makeFakeBlob(); });
    fabric::EventDispatcher dispatcher;

    dispatcher.addEventListener(recurse::K_VOXEL_CHANGED_EVENT, [&svc](fabric::Event& e) {
        int cx = e.getData<int>("cx");
        int cy = e.getData<int>("cy");
        int cz = e.getData<int>("cz");
        svc.markDirty(cx, cy, cz);
    });

    EXPECT_EQ(svc.pendingCount(), 0u);

    recurse::emitVoxelChanged(dispatcher, 1, 2, 3);
    EXPECT_EQ(svc.pendingCount(), 1u);

    recurse::emitVoxelChanged(dispatcher, 4, 5, 6);
    EXPECT_EQ(svc.pendingCount(), 2u);
}

TEST_F(EditTriggeredSaveTest, UpdateDrivesDebouncedSaves) {
    recurse::ChunkSaveService svc(*store_, writerQueue_, [&](int, int, int) { return makeFakeBlob(); });
    svc.debounceSeconds = 1.0f;
    svc.maxDelaySeconds = 5.0f;

    fabric::EventDispatcher dispatcher;
    dispatcher.addEventListener(recurse::K_VOXEL_CHANGED_EVENT, [&svc](fabric::Event& e) {
        svc.markDirty(e.getData<int>("cx"), e.getData<int>("cy"), e.getData<int>("cz"));
    });

    recurse::emitVoxelChanged(dispatcher, 0, 0, 0);

    svc.update(0.5f);
    EXPECT_FALSE(store_->hasChunk(0, 0, 0));

    svc.update(0.6f);
    writerQueue_.drain();
    EXPECT_TRUE(store_->hasChunk(0, 0, 0));
    EXPECT_EQ(svc.pendingCount(), 0u);
}

TEST_F(EditTriggeredSaveTest, MultipleEventsMultipleChunks) {
    recurse::ChunkSaveService svc(*store_, writerQueue_, [&](int, int, int) { return makeFakeBlob(); });

    fabric::EventDispatcher dispatcher;
    dispatcher.addEventListener(recurse::K_VOXEL_CHANGED_EVENT, [&svc](fabric::Event& e) {
        svc.markDirty(e.getData<int>("cx"), e.getData<int>("cy"), e.getData<int>("cz"));
    });

    recurse::emitVoxelChanged(dispatcher, 0, 0, 0);
    recurse::emitVoxelChanged(dispatcher, 1, 1, 1);
    recurse::emitVoxelChanged(dispatcher, -2, 3, -4);
    EXPECT_EQ(svc.pendingCount(), 3u);

    svc.flush();
    EXPECT_EQ(svc.pendingCount(), 0u);
    EXPECT_TRUE(store_->hasChunk(0, 0, 0));
    EXPECT_TRUE(store_->hasChunk(1, 1, 1));
    EXPECT_TRUE(store_->hasChunk(-2, 3, -4));
}

TEST_F(EditTriggeredSaveTest, NoSaveServiceNoOp) {
    fabric::EventDispatcher dispatcher;
    recurse::ChunkSaveService* nullSvc = nullptr;

    dispatcher.addEventListener(recurse::K_VOXEL_CHANGED_EVENT, [&nullSvc](fabric::Event& e) {
        if (nullSvc)
            nullSvc->markDirty(e.getData<int>("cx"), e.getData<int>("cy"), e.getData<int>("cz"));
    });

    recurse::emitVoxelChanged(dispatcher, 0, 0, 0);
    SUCCEED();
}

TEST_F(EditTriggeredSaveTest, SummaryEventCarriesWorldChangeEnvelope) {
    fabric::EventDispatcher dispatcher;

    recurse::WorldChangeEnvelope envelope;
    bool sawEvent = false;
    auto listenerId = dispatcher.addEventListener(recurse::K_VOXEL_CHANGED_EVENT, [&](fabric::Event& e) {
        sawEvent = true;
        ASSERT_TRUE(e.hasAnyData(recurse::K_WORLD_CHANGE_ENVELOPE_KEY));
        envelope = e.getAnyData<recurse::WorldChangeEnvelope>(recurse::K_WORLD_CHANGE_ENVELOPE_KEY);
    });

    recurse::emitChunkChangeSummary(dispatcher, 4, 5, 6, recurse::ChangeSource::Generation,
                                    recurse::FunctionHistoryMode::SnapshotOnly,
                                    recurse::FunctionCostClass::ChunkLinear);

    ASSERT_TRUE(sawEvent);
    EXPECT_EQ(envelope.source, recurse::ChangeSource::Generation);
    EXPECT_EQ(envelope.targetKind, recurse::FunctionTargetKind::Chunk);
    EXPECT_EQ(envelope.historyMode, recurse::FunctionHistoryMode::SnapshotOnly);
    ASSERT_EQ(envelope.touchedChunks.size(), 1u);
    EXPECT_EQ(envelope.touchedChunks[0], (fabric::ChunkCoord{4, 5, 6}));
    EXPECT_TRUE(envelope.voxelDeltas.empty());
    ASSERT_EQ(envelope.chunkDeltas.size(), 1u);
    EXPECT_EQ(envelope.chunkDeltas[0].chunk, (fabric::ChunkCoord{4, 5, 6}));

    dispatcher.removeEventListener(recurse::K_VOXEL_CHANGED_EVENT, listenerId);
}
