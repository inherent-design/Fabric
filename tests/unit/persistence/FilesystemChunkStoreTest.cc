#include "recurse/persistence/FilesystemChunkStore.hh"

#include <cstring>
#include <filesystem>
#include <gtest/gtest.h>

namespace fs = std::filesystem;

class FilesystemChunkStoreTest : public ::testing::Test {
  protected:
    void SetUp() override {
        tmpDir_ = fs::temp_directory_path() / "fabric_test_chunkstore";
        fs::remove_all(tmpDir_);
        fs::create_directories(tmpDir_);
        worldDir_ = (tmpDir_ / "testworld").string();
    }
    void TearDown() override { fs::remove_all(tmpDir_); }

    fs::path tmpDir_;
    std::string worldDir_;

    // Create a fake 128KB chunk payload (32^3 * 4 bytes)
    static recurse::ChunkBlob makeFakeChunkData() {
        constexpr size_t K_CHUNK_VOLUME = 32 * 32 * 32;
        constexpr size_t K_CELL_SIZE = 4;
        std::vector<uint8_t> cells(K_CHUNK_VOLUME * K_CELL_SIZE, 0);
        // Write a recognizable pattern: first cell = materialId 42
        cells[0] = 42;
        cells[1] = 0;
        return recurse::FilesystemChunkStore::encode(cells.data(), cells.size());
    }
};

TEST_F(FilesystemChunkStoreTest, ConstructorCreatesDirectories) {
    recurse::FilesystemChunkStore store(worldDir_);
    EXPECT_TRUE(fs::is_directory(worldDir_ + "/chunks/gen"));
    EXPECT_TRUE(fs::is_directory(worldDir_ + "/chunks/delta"));
}

TEST_F(FilesystemChunkStoreTest, HasGenDataReturnsFalseForMissing) {
    recurse::FilesystemChunkStore store(worldDir_);
    EXPECT_FALSE(store.hasGenData(0, 0, 0));
}

TEST_F(FilesystemChunkStoreTest, SaveAndLoadGenData) {
    recurse::FilesystemChunkStore store(worldDir_);
    auto blob = makeFakeChunkData();

    store.saveGenData(1, 2, 3, blob);
    EXPECT_TRUE(store.hasGenData(1, 2, 3));

    auto loaded = store.loadGenData(1, 2, 3);
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->size(), blob.size());
    EXPECT_EQ(*loaded, blob);
}

TEST_F(FilesystemChunkStoreTest, SaveAndLoadDelta) {
    recurse::FilesystemChunkStore store(worldDir_);
    auto blob = makeFakeChunkData();

    store.saveDelta(0, 0, 0, blob);
    EXPECT_TRUE(store.hasDelta(0, 0, 0));

    auto loaded = store.loadDelta(0, 0, 0);
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(*loaded, blob);
}

TEST_F(FilesystemChunkStoreTest, NegativeCoordinates) {
    recurse::FilesystemChunkStore store(worldDir_);
    auto blob = makeFakeChunkData();

    store.saveGenData(-5, -10, 3, blob);
    EXPECT_TRUE(store.hasGenData(-5, -10, 3));

    auto loaded = store.loadGenData(-5, -10, 3);
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(*loaded, blob);
}

TEST_F(FilesystemChunkStoreTest, CompactMovesDeltaToGen) {
    recurse::FilesystemChunkStore store(worldDir_);
    auto genBlob = makeFakeChunkData();
    auto deltaBlob = makeFakeChunkData();
    // Make delta different
    deltaBlob[sizeof(recurse::FchkHeader)] = 99;

    store.saveGenData(0, 0, 0, genBlob);
    store.saveDelta(0, 0, 0, deltaBlob);

    EXPECT_TRUE(store.hasGenData(0, 0, 0));
    EXPECT_TRUE(store.hasDelta(0, 0, 0));

    store.compactChunk(0, 0, 0);

    EXPECT_TRUE(store.hasGenData(0, 0, 0));
    EXPECT_FALSE(store.hasDelta(0, 0, 0));

    // Gen data should now contain delta contents
    auto loaded = store.loadGenData(0, 0, 0);
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(*loaded, deltaBlob);
}

TEST_F(FilesystemChunkStoreTest, CompactNoopWithoutDelta) {
    recurse::FilesystemChunkStore store(worldDir_);
    auto blob = makeFakeChunkData();
    store.saveGenData(0, 0, 0, blob);

    // Should not throw or crash
    store.compactChunk(0, 0, 0);

    auto loaded = store.loadGenData(0, 0, 0);
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(*loaded, blob);
}

TEST_F(FilesystemChunkStoreTest, SizeQueries) {
    recurse::FilesystemChunkStore store(worldDir_);
    EXPECT_EQ(store.genDataSize(0, 0, 0), 0u);
    EXPECT_EQ(store.deltaSize(0, 0, 0), 0u);

    auto blob = makeFakeChunkData();
    store.saveGenData(0, 0, 0, blob);
    EXPECT_EQ(store.genDataSize(0, 0, 0), blob.size());
}

TEST_F(FilesystemChunkStoreTest, FchkEncodeDecodeRoundTrip) {
    constexpr size_t payloadSize = 32 * 32 * 32 * 4;
    std::vector<uint8_t> cells(payloadSize);
    cells[0] = 0xAB;
    cells[payloadSize - 1] = 0xCD;

    auto blob = recurse::FilesystemChunkStore::encode(cells.data(), payloadSize);
    // v2 blob: header + payload + paletteCount (uint16_t, value 0)
    EXPECT_EQ(blob.size(), sizeof(recurse::FchkHeader) + payloadSize + sizeof(uint16_t));

    auto decoded = recurse::FilesystemChunkStore::decode(blob);
    EXPECT_EQ(decoded.cells.size(), payloadSize);
    EXPECT_EQ(decoded.cells[0], 0xAB);
    EXPECT_EQ(decoded.cells[payloadSize - 1], 0xCD);
    EXPECT_EQ(decoded.paletteEntryCount, 0u);
}

TEST_F(FilesystemChunkStoreTest, FchkDecodeRejectsBadMagic) {
    recurse::ChunkBlob blob(20, 0);
    blob[0] = 'X'; // Wrong magic
    EXPECT_THROW(recurse::FilesystemChunkStore::decode(blob), std::exception);
}

TEST_F(FilesystemChunkStoreTest, FchkDecodeRejectsTooSmall) {
    recurse::ChunkBlob blob(5, 0);
    EXPECT_THROW(recurse::FilesystemChunkStore::decode(blob), std::exception);
}

TEST_F(FilesystemChunkStoreTest, LoadMissingReturnsNullopt) {
    recurse::FilesystemChunkStore store(worldDir_);
    auto result = store.loadGenData(999, 999, 999);
    EXPECT_FALSE(result.has_value());
}
