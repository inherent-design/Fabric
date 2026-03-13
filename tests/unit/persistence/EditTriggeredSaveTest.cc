#include "fabric/core/Event.hh"
#include "fabric/platform/JobScheduler.hh"
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
        jobs_ = std::make_unique<fabric::JobScheduler>(1);
        jobs_->disableForTesting();
    }
    void TearDown() override { fs::remove_all(tmpDir_); }

    fs::path tmpDir_;
    std::string worldDir_;
    std::unique_ptr<recurse::SqliteChunkStore> store_;
    std::unique_ptr<fabric::JobScheduler> jobs_;

    static recurse::ChunkBlob makeFakeBlob(uint8_t marker = 0xAA) {
        constexpr size_t payloadSize = 32 * 32 * 32 * 4;
        std::vector<uint8_t> cells(payloadSize, 0);
        cells[0] = marker;
        return recurse::FchkCodec::encode(cells.data(), cells.size());
    }
};

TEST_F(EditTriggeredSaveTest, EventTriggersMarkDirty) {
    recurse::ChunkSaveService svc(*store_, *jobs_, [&](int, int, int) { return makeFakeBlob(); });
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
    recurse::ChunkSaveService svc(*store_, *jobs_, [&](int, int, int) { return makeFakeBlob(); });
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
    EXPECT_TRUE(store_->hasChunk(0, 0, 0));
    EXPECT_EQ(svc.pendingCount(), 0u);
}

TEST_F(EditTriggeredSaveTest, MultipleEventsMultipleChunks) {
    recurse::ChunkSaveService svc(*store_, *jobs_, [&](int, int, int) { return makeFakeBlob(); });

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
