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

// --- Batch operations ---

TEST_F(FilesystemChunkStoreTest, BatchLoadMultipleChunks) {
    recurse::FilesystemChunkStore store(worldDir_);
    auto blob = makeFakeChunkData();

    store.saveGenData(0, 0, 0, blob);
    store.saveGenData(1, 0, 0, blob);
    store.saveGenData(2, 0, 0, blob);

    std::vector<std::tuple<int, int, int>> coords = {{0, 0, 0}, {1, 0, 0}, {2, 0, 0}};
    auto results = store.loadBatch(coords);

    EXPECT_EQ(results.size(), 3u);
    for (const auto& [coord, loaded] : results) {
        EXPECT_EQ(loaded.size(), blob.size());
        EXPECT_EQ(loaded, blob);
    }
}

TEST_F(FilesystemChunkStoreTest, BatchLoadMixedExistence) {
    recurse::FilesystemChunkStore store(worldDir_);
    auto blob = makeFakeChunkData();

    store.saveGenData(0, 0, 0, blob);
    store.saveGenData(3, 3, 3, blob);

    std::vector<std::tuple<int, int, int>> coords = {{0, 0, 0}, {1, 1, 1}, {2, 2, 2}, {3, 3, 3}};
    auto results = store.loadBatch(coords);

    EXPECT_EQ(results.size(), 2u);
}

TEST_F(FilesystemChunkStoreTest, BatchSaveMultiple) {
    recurse::FilesystemChunkStore store(worldDir_);
    auto blob = makeFakeChunkData();

    std::vector<std::pair<std::tuple<int, int, int>, recurse::ChunkBlob>> entries;
    entries.push_back({{0, 0, 0}, blob});
    entries.push_back({{1, 2, 3}, blob});
    entries.push_back({{-1, -1, 0}, blob});

    store.saveBatch(entries);

    EXPECT_TRUE(store.hasGenData(0, 0, 0));
    EXPECT_TRUE(store.hasGenData(1, 2, 3));
    EXPECT_TRUE(store.hasGenData(-1, -1, 0));

    auto loaded = store.loadGenData(1, 2, 3);
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(*loaded, blob);
}

TEST_F(FilesystemChunkStoreTest, BatchSaveEmpty) {
    recurse::FilesystemChunkStore store(worldDir_);
    std::vector<std::pair<std::tuple<int, int, int>, recurse::ChunkBlob>> entries;
    store.saveBatch(entries);
}

// --- Group C: FCHK v2 encode/decode ---

TEST_F(FilesystemChunkStoreTest, FchkV2EncodeDecodeWithPalette) {
    constexpr size_t cellsSize = 32 * 32 * 32 * 4;
    std::vector<uint8_t> cells(cellsSize, 0);
    cells[0] = 0xAA;
    cells[cellsSize - 1] = 0xBB;

    float paletteData[] = {
        0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f, 0.0f, 0.0f,
    };

    auto blob = recurse::FilesystemChunkStore::encode(cells.data(), cellsSize, 0, 1, paletteData, 3);

    auto decoded = recurse::FilesystemChunkStore::decode(blob);
    EXPECT_EQ(decoded.cells.size(), cellsSize);
    EXPECT_EQ(decoded.cells[0], 0xAA);
    EXPECT_EQ(decoded.cells[cellsSize - 1], 0xBB);
    EXPECT_EQ(decoded.paletteEntryCount, 3u);
    ASSERT_EQ(decoded.paletteData.size(), 12u);
    for (int i = 0; i < 12; ++i) {
        EXPECT_FLOAT_EQ(decoded.paletteData[i], paletteData[i]) << "palette float " << i;
    }
}

TEST_F(FilesystemChunkStoreTest, FchkV2EncodeDecodeEmptyPalette) {
    constexpr size_t cellsSize = 32 * 32 * 32 * 4;
    std::vector<uint8_t> cells(cellsSize, 0);
    cells[0] = 0x42;

    auto blob = recurse::FilesystemChunkStore::encode(cells.data(), cellsSize, 0, 1, nullptr, 0);

    auto decoded = recurse::FilesystemChunkStore::decode(blob);
    EXPECT_EQ(decoded.cells.size(), cellsSize);
    EXPECT_EQ(decoded.cells[0], 0x42);
    EXPECT_EQ(decoded.paletteEntryCount, 0u);
    EXPECT_TRUE(decoded.paletteData.empty());
}

TEST_F(FilesystemChunkStoreTest, FchkV2EncodeDecodeMaxPalette) {
    constexpr size_t cellsSize = 32 * 32 * 32 * 4;
    std::vector<uint8_t> cells(cellsSize, 0);

    std::vector<float> paletteData(256 * 4);
    for (int i = 0; i < 256; ++i) {
        paletteData[i * 4 + 0] = static_cast<float>(i) / 255.0f;
        paletteData[i * 4 + 1] = 1.0f - static_cast<float>(i) / 255.0f;
        paletteData[i * 4 + 2] = 0.5f;
        paletteData[i * 4 + 3] = 0.0f;
    }

    auto blob = recurse::FilesystemChunkStore::encode(cells.data(), cellsSize, 0, 1, paletteData.data(), 256);

    // 10 (header) + 131072 (cells) + 2 (count) + 256*16 (palette) = 135180
    EXPECT_EQ(blob.size(), 135180u);

    auto decoded = recurse::FilesystemChunkStore::decode(blob);
    EXPECT_EQ(decoded.paletteEntryCount, 256u);
    ASSERT_EQ(decoded.paletteData.size(), 256u * 4);
    for (size_t i = 0; i < paletteData.size(); ++i) {
        EXPECT_FLOAT_EQ(decoded.paletteData[i], paletteData[i]) << "float " << i;
    }
}

TEST_F(FilesystemChunkStoreTest, FchkV1BackwardsCompat) {
    constexpr size_t cellsSize = 32 * 32 * 32 * 4;

    // Build a v1 blob manually: header + raw cells, no palette section
    recurse::FchkHeader hdr;
    hdr.version = 1;
    hdr.compression = 0;

    std::vector<uint8_t> cells(cellsSize, 0);
    // Set recognizable pattern: materialId=42 and essenceIdx=0xFF in cell 0
    cells[0] = 42;   // materialId low byte
    cells[1] = 0;    // materialId high byte
    cells[2] = 0xFF; // essenceIdx (should be zeroed by v1 fixup)
    cells[3] = 0;    // flags

    recurse::ChunkBlob blob(sizeof(recurse::FchkHeader) + cellsSize);
    std::memcpy(blob.data(), &hdr, sizeof(recurse::FchkHeader));
    std::memcpy(blob.data() + sizeof(recurse::FchkHeader), cells.data(), cellsSize);

    auto decoded = recurse::FilesystemChunkStore::decode(blob);
    EXPECT_EQ(decoded.cells.size(), cellsSize);
    EXPECT_EQ(decoded.cells[0], 42u);
    EXPECT_EQ(decoded.cells[2], 0u); // v1 fixup zeroed essenceIdx
    EXPECT_EQ(decoded.paletteEntryCount, 0u);
}

TEST_F(FilesystemChunkStoreTest, FchkV2RejectsFutureVersion) {
    constexpr size_t cellsSize = 32 * 32 * 32 * 4;

    recurse::FchkHeader hdr;
    hdr.version = 3;
    hdr.compression = 0;

    recurse::ChunkBlob blob(sizeof(recurse::FchkHeader) + cellsSize);
    std::memcpy(blob.data(), &hdr, sizeof(recurse::FchkHeader));

    EXPECT_THROW(recurse::FilesystemChunkStore::decode(blob), std::exception);
}

TEST_F(FilesystemChunkStoreTest, FchkV2PaletteDataIntegrity) {
    constexpr size_t cellsSize = 32 * 32 * 32 * 4;
    std::vector<uint8_t> cells(cellsSize, 0);

    float paletteData[] = {
        0.8f, 0.0f, 0.0f, 0.2f, 0.0f, 0.7f, 0.3f, 0.0f,
    };

    auto blob = recurse::FilesystemChunkStore::encode(cells.data(), cellsSize, 0, 1, paletteData, 2);

    auto decoded = recurse::FilesystemChunkStore::decode(blob);
    ASSERT_EQ(decoded.paletteEntryCount, 2u);
    ASSERT_EQ(decoded.paletteData.size(), 8u);
    EXPECT_FLOAT_EQ(decoded.paletteData[0], 0.8f);
    EXPECT_FLOAT_EQ(decoded.paletteData[1], 0.0f);
    EXPECT_FLOAT_EQ(decoded.paletteData[2], 0.0f);
    EXPECT_FLOAT_EQ(decoded.paletteData[3], 0.2f);
    EXPECT_FLOAT_EQ(decoded.paletteData[4], 0.0f);
    EXPECT_FLOAT_EQ(decoded.paletteData[5], 0.7f);
    EXPECT_FLOAT_EQ(decoded.paletteData[6], 0.3f);
    EXPECT_FLOAT_EQ(decoded.paletteData[7], 0.0f);
}

// --- Group D: Save/load palette persistence ---

TEST_F(FilesystemChunkStoreTest, PalettePersistsThroughSaveLoad) {
    recurse::FilesystemChunkStore store(worldDir_);

    constexpr size_t cellsSize = 32 * 32 * 32 * 4;
    std::vector<uint8_t> cells(cellsSize, 0);
    float paletteData[] = {
        0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f,
    };

    auto blob = recurse::FilesystemChunkStore::encode(cells.data(), cellsSize, 0, 1, paletteData, 2);

    store.saveGenData(0, 0, 0, blob);
    auto loaded = store.loadGenData(0, 0, 0);
    ASSERT_TRUE(loaded.has_value());

    auto decoded = recurse::FilesystemChunkStore::decode(*loaded);
    EXPECT_EQ(decoded.paletteEntryCount, 2u);
    ASSERT_EQ(decoded.paletteData.size(), 8u);
    for (int i = 0; i < 8; ++i) {
        EXPECT_FLOAT_EQ(decoded.paletteData[i], paletteData[i]) << "float " << i;
    }
}

TEST_F(FilesystemChunkStoreTest, V1FileLoadsWithNoPalette) {
    recurse::FilesystemChunkStore store(worldDir_);

    constexpr size_t cellsSize = 32 * 32 * 32 * 4;
    recurse::FchkHeader hdr;
    hdr.version = 1;
    hdr.compression = 0;

    std::vector<uint8_t> cells(cellsSize, 0);
    cells[2] = 0xAB; // essenceIdx byte, should be zeroed

    recurse::ChunkBlob blob(sizeof(recurse::FchkHeader) + cellsSize);
    std::memcpy(blob.data(), &hdr, sizeof(recurse::FchkHeader));
    std::memcpy(blob.data() + sizeof(recurse::FchkHeader), cells.data(), cellsSize);

    store.saveGenData(0, 0, 0, blob);
    auto loaded = store.loadGenData(0, 0, 0);
    ASSERT_TRUE(loaded.has_value());

    auto decoded = recurse::FilesystemChunkStore::decode(*loaded);
    EXPECT_EQ(decoded.paletteEntryCount, 0u);
    EXPECT_EQ(decoded.cells[2], 0u);
}

TEST_F(FilesystemChunkStoreTest, PaletteRoundTripThroughCompaction) {
    recurse::FilesystemChunkStore store(worldDir_);

    constexpr size_t cellsSize = 32 * 32 * 32 * 4;
    std::vector<uint8_t> cells(cellsSize, 0);

    float paletteA[] = {1.0f, 0.0f, 0.0f, 0.0f};
    float paletteB[] = {0.0f, 1.0f, 0.0f, 0.0f};

    auto blobA = recurse::FilesystemChunkStore::encode(cells.data(), cellsSize, 0, 1, paletteA, 1);
    auto blobB = recurse::FilesystemChunkStore::encode(cells.data(), cellsSize, 0, 1, paletteB, 1);

    store.saveGenData(0, 0, 0, blobA);
    store.saveDelta(0, 0, 0, blobB);
    store.compactChunk(0, 0, 0);

    auto loaded = store.loadGenData(0, 0, 0);
    ASSERT_TRUE(loaded.has_value());

    auto decoded = recurse::FilesystemChunkStore::decode(*loaded);
    EXPECT_EQ(decoded.paletteEntryCount, 1u);
    ASSERT_EQ(decoded.paletteData.size(), 4u);
    EXPECT_FLOAT_EQ(decoded.paletteData[0], 0.0f);
    EXPECT_FLOAT_EQ(decoded.paletteData[1], 1.0f);
    EXPECT_FLOAT_EQ(decoded.paletteData[2], 0.0f);
    EXPECT_FLOAT_EQ(decoded.paletteData[3], 0.0f);
}
