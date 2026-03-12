#include "recurse/persistence/SqliteChunkStore.hh"
#include "recurse/persistence/FchkCodec.hh"
#include "recurse/persistence/SchemaMigrations.hh"

#include <cstring>
#include <filesystem>
#include <gtest/gtest.h>
#include <sqlite3.h>

namespace fs = std::filesystem;

class SqliteChunkStoreTest : public ::testing::Test {
  protected:
    void SetUp() override {
        worldDir_ =
            fs::temp_directory_path() /
            ("fabric_sqlite_test_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        fs::create_directories(worldDir_);
    }

    void TearDown() override { fs::remove_all(worldDir_); }

    std::string worldDir() const { return worldDir_.string(); }

    static recurse::ChunkBlob makeFakeBlob() {
        constexpr size_t K_CHUNK_VOLUME = 32 * 32 * 32;
        constexpr size_t K_CELL_SIZE = 4;
        std::vector<uint8_t> cells(K_CHUNK_VOLUME * K_CELL_SIZE, 0);
        cells[0] = 42;
        return recurse::FchkCodec::encode(cells.data(), cells.size());
    }

  private:
    fs::path worldDir_;
};

// ---------------------------------------------------------------------------
// Group A: Schema + Logic
// ---------------------------------------------------------------------------

TEST_F(SqliteChunkStoreTest, SchemaCreation) {
    { recurse::SqliteChunkStore store(worldDir()); }

    std::string dbPath = worldDir() + "/world.db";
    ASSERT_TRUE(fs::exists(dbPath));

    sqlite3* db = nullptr;
    ASSERT_EQ(sqlite3_open(dbPath.c_str(), &db), SQLITE_OK);

    // Verify PRAGMA user_version matches K_SCHEMA_VERSION.
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db, "PRAGMA user_version", -1, &stmt, nullptr);
    ASSERT_EQ(sqlite3_step(stmt), SQLITE_ROW);
    int version = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);
    EXPECT_EQ(version, recurse::persistence::K_SCHEMA_VERSION);

    // Verify all 4 tables exist.
    auto tableExists = [&](const char* name) -> bool {
        sqlite3_stmt* s = nullptr;
        sqlite3_prepare_v2(db, "SELECT 1 FROM sqlite_master WHERE type='table' AND name=?1", -1, &s, nullptr);
        sqlite3_bind_text(s, 1, name, -1, SQLITE_STATIC);
        bool found = (sqlite3_step(s) == SQLITE_ROW);
        sqlite3_finalize(s);
        return found;
    };

    EXPECT_TRUE(tableExists("chunk_state"));
    EXPECT_TRUE(tableExists("change_log"));
    EXPECT_TRUE(tableExists("chunk_snapshot"));
    EXPECT_TRUE(tableExists("schema_version"));

    // Verify 3 indexes exist.
    auto indexExists = [&](const char* name) -> bool {
        sqlite3_stmt* s = nullptr;
        sqlite3_prepare_v2(db, "SELECT 1 FROM sqlite_master WHERE type='index' AND name=?1", -1, &s, nullptr);
        sqlite3_bind_text(s, 1, name, -1, SQLITE_STATIC);
        bool found = (sqlite3_step(s) == SQLITE_ROW);
        sqlite3_finalize(s);
        return found;
    };

    EXPECT_TRUE(indexExists("idx_changelog_chunk_time"));
    EXPECT_TRUE(indexExists("idx_changelog_player_time"));
    EXPECT_TRUE(indexExists("idx_snapshot_chunk_time"));

    sqlite3_close(db);
}

TEST_F(SqliteChunkStoreTest, PragmaConfiguration) {
    { recurse::SqliteChunkStore store(worldDir()); }

    // journal_mode=WAL is persistent in the DB file. Verify via raw connection.
    std::string dbPath = worldDir() + "/world.db";
    sqlite3* db = nullptr;
    ASSERT_EQ(sqlite3_open(dbPath.c_str(), &db), SQLITE_OK);

    sqlite3_stmt* s = nullptr;
    sqlite3_prepare_v2(db, "PRAGMA journal_mode", -1, &s, nullptr);
    ASSERT_EQ(sqlite3_step(s), SQLITE_ROW);
    std::string journalMode = reinterpret_cast<const char*>(sqlite3_column_text(s, 0));
    sqlite3_finalize(s);

    EXPECT_EQ(journalMode, "wal");

    sqlite3_close(db);
}

TEST_F(SqliteChunkStoreTest, SaveLoadRoundTrip) {
    recurse::SqliteChunkStore store(worldDir());
    auto blob = makeFakeBlob();

    store.saveChunk(1, 2, 3, blob);

    EXPECT_TRUE(store.hasChunk(1, 2, 3));
    EXPECT_EQ(store.chunkSize(1, 2, 3), blob.size());

    auto loaded = store.loadChunk(1, 2, 3);
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->size(), blob.size());
    EXPECT_EQ(loaded->data, blob.data);
}

TEST_F(SqliteChunkStoreTest, SaveLoadWithFchkDecode) {
    recurse::SqliteChunkStore store(worldDir());

    constexpr size_t cellsSize = 32 * 32 * 32 * 4;
    std::vector<uint8_t> cells(cellsSize, 0);
    cells[0] = 0xAA;
    cells[cellsSize - 1] = 0xBB;

    float paletteData[] = {
        0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f,
    };

    auto blob = recurse::FchkCodec::encode(cells.data(), cellsSize, 0, 1, paletteData, 2);
    store.saveChunk(5, 6, 7, blob);

    auto loaded = store.loadChunk(5, 6, 7);
    ASSERT_TRUE(loaded.has_value());

    auto decoded = recurse::FchkCodec::decode(*loaded);
    EXPECT_EQ(decoded.cells.size(), cellsSize);
    EXPECT_EQ(decoded.cells[0], 0xAA);
    EXPECT_EQ(decoded.cells[cellsSize - 1], 0xBB);
    EXPECT_EQ(decoded.paletteEntryCount, 2u);
    ASSERT_EQ(decoded.paletteData.size(), 8u);
    for (int i = 0; i < 8; ++i) {
        EXPECT_FLOAT_EQ(decoded.paletteData[i], paletteData[i]) << "palette float " << i;
    }
}

TEST_F(SqliteChunkStoreTest, BatchOperations) {
    recurse::SqliteChunkStore store(worldDir());
    auto blob = makeFakeBlob();

    std::vector<std::pair<fabric::ChunkCoord, recurse::ChunkBlob>> entries;
    for (int i = 0; i < 5; ++i) {
        entries.push_back({{i, 0, 0}, blob});
    }
    store.saveBatch(entries);

    std::vector<fabric::ChunkCoord> coords;
    for (int i = 0; i < 5; ++i) {
        coords.push_back({i, 0, 0});
    }
    auto results = store.loadBatch(coords);
    EXPECT_EQ(results.size(), 5u);

    for (const auto& [coord, loaded] : results) {
        EXPECT_EQ(loaded.data, blob.data);
    }

    // Load a batch that includes non-existent coords.
    std::vector<fabric::ChunkCoord> mixed = {{0, 0, 0}, {99, 99, 99}, {2, 0, 0}, {100, 100, 100}};
    auto mixedResults = store.loadBatch(mixed);
    EXPECT_EQ(mixedResults.size(), 2u);
}

TEST_F(SqliteChunkStoreTest, OverwriteExisting) {
    recurse::SqliteChunkStore store(worldDir());

    constexpr size_t cellsSize = 32 * 32 * 32 * 4;
    std::vector<uint8_t> cellsA(cellsSize, 0);
    cellsA[0] = 0x11;
    auto blobA = recurse::FchkCodec::encode(cellsA.data(), cellsA.size());

    std::vector<uint8_t> cellsB(cellsSize, 0);
    cellsB[0] = 0x22;
    auto blobB = recurse::FchkCodec::encode(cellsB.data(), cellsB.size());

    store.saveChunk(1, 1, 1, blobA);
    store.saveChunk(1, 1, 1, blobB);

    auto loaded = store.loadChunk(1, 1, 1);
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->data, blobB.data);
}

// ---------------------------------------------------------------------------
// Group B: Concurrency + Lifecycle
// ---------------------------------------------------------------------------

TEST_F(SqliteChunkStoreTest, ChunkNotFound) {
    recurse::SqliteChunkStore store(worldDir());

    EXPECT_FALSE(store.hasChunk(999, 888, 777));

    auto result = store.loadChunk(999, 888, 777);
    EXPECT_FALSE(result.has_value());

    EXPECT_EQ(store.chunkSize(999, 888, 777), 0u);
}

TEST_F(SqliteChunkStoreTest, EmptyBlobSkipsSave) {
    recurse::SqliteChunkStore store(worldDir());

    recurse::ChunkBlob emptyBlob;
    store.saveChunk(0, 0, 0, emptyBlob);

    EXPECT_FALSE(store.hasChunk(0, 0, 0));
}

TEST_F(SqliteChunkStoreTest, CleanShutdown) {
    std::string dbPath = worldDir() + "/world.db";

    {
        recurse::SqliteChunkStore store(worldDir());
        auto blob = makeFakeBlob();
        store.saveChunk(1, 2, 3, blob);
        store.close();

        EXPECT_FALSE(fs::exists(dbPath + "-wal"));
        EXPECT_FALSE(fs::exists(dbPath + "-shm"));
    }

    // Re-open and verify data survived.
    recurse::SqliteChunkStore store2(worldDir());
    EXPECT_TRUE(store2.hasChunk(1, 2, 3));

    auto loaded = store2.loadChunk(1, 2, 3);
    ASSERT_TRUE(loaded.has_value());
    EXPECT_FALSE(loaded->empty());
}

// ---------------------------------------------------------------------------
// Group C: Schema Edge Cases
// ---------------------------------------------------------------------------

TEST_F(SqliteChunkStoreTest, SchemaDowngradeRefused) {
    { recurse::SqliteChunkStore store(worldDir()); }

    std::string dbPath = worldDir() + "/world.db";
    sqlite3* db = nullptr;
    ASSERT_EQ(sqlite3_open(dbPath.c_str(), &db), SQLITE_OK);

    int futureVersion = recurse::persistence::K_SCHEMA_VERSION + 1;
    std::string sql = "PRAGMA user_version = " + std::to_string(futureVersion);
    ASSERT_EQ(sqlite3_exec(db, sql.c_str(), nullptr, nullptr, nullptr), SQLITE_OK);

    // Flush to main DB file so the version persists after close.
    int nLog = 0, nCkpt = 0;
    sqlite3_wal_checkpoint_v2(db, nullptr, SQLITE_CHECKPOINT_TRUNCATE, &nLog, &nCkpt);
    sqlite3_close(db);

    // Manual try-catch instead of EXPECT_THROW. The macro interacts poorly
    // with SqliteChunkStore's destructor when construction throws mid-way
    // (the partially-constructed temporary confuses scope cleanup).
    bool threwException = false;
    try {
        recurse::SqliteChunkStore store2(worldDir());
    } catch (const std::exception&) {
        threwException = true;
    }
    EXPECT_TRUE(threwException) << "SqliteChunkStore should throw when schema version is newer than supported";
}

TEST_F(SqliteChunkStoreTest, NegativeCoordinates) {
    recurse::SqliteChunkStore store(worldDir());
    auto blob = makeFakeBlob();

    store.saveChunk(-5, -10, -15, blob);
    EXPECT_TRUE(store.hasChunk(-5, -10, -15));

    auto loaded = store.loadChunk(-5, -10, -15);
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->data, blob.data);
}
