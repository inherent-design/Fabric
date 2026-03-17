#pragma once

#include "recurse/persistence/ChunkStore.hh"
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <unordered_set>

// Forward-declare sqlite3 types to avoid exposing sqlite3.h in the header.
struct sqlite3;
struct sqlite3_stmt;

namespace recurse {

/// SQLite-backed ChunkStore. Single world.db per world.
///
/// Two connections: writer (owns schema, saves) and reader (loads, queries).
/// WAL mode allows concurrent read/write without blocking.
class SqliteChunkStore : public ChunkStore {
  public:
    /// Open or create world.db at {worldDir}/world.db.
    /// Runs schema migrations on writer connection.
    explicit SqliteChunkStore(const std::string& worldDir);
    ~SqliteChunkStore() override;

    // Non-copyable, non-movable (owns sqlite3 handles).
    SqliteChunkStore(const SqliteChunkStore&) = delete;
    SqliteChunkStore& operator=(const SqliteChunkStore&) = delete;

    // --- ChunkStore v2 API ---

    bool hasChunk(int cx, int cy, int cz) const override;
    std::optional<ChunkBlob> loadChunk(int cx, int cy, int cz) const override;
    void saveChunk(int cx, int cy, int cz, const ChunkBlob& data) override;
    size_t chunkSize(int cx, int cy, int cz) const override;

    std::vector<std::pair<fabric::ChunkCoord, ChunkBlob>>
    loadBatch(const std::vector<fabric::ChunkCoord>& coords) const override;
    void saveBatch(const std::vector<std::pair<fabric::ChunkCoord, ChunkBlob>>& entries) override;

    /// Run PASSIVE WAL checkpoint. Call periodically (every ~5s) from writer thread.
    void maybeCheckpoint();

    /// Close connections cleanly. TRUNCATE checkpoint on writer after closing reader.
    /// Called automatically by destructor, but can be called explicitly.
    void close();

    /// Raw handle access for co-located stores sharing this database.
    sqlite3* writerDb() const { return writerDb_; }
    sqlite3* readerDb() const { return readerDb_; }

    /// Check if a coordinate is in the set of saved chunk coords.
    /// False: definitely not saved. True: chunk was saved (or is being saved).
    /// Thread-safe: guarded by shared_mutex (reader/writer).
    bool isInSavedRegion(int cx, int cy, int cz) const;

    /// Set worldgen version for delta persistence. Bound to parameter 6 on save.
    void setWorldgenVersion(uint32_t version);

  private:
    void openConnections(const std::string& dbPath);
    void configurePragmas(sqlite3* db);
    void runMigrations();
    void prepareStatements();
    void finalizeStatements();

    void execSql(sqlite3* db, const char* sql);

    std::string dbPath_;

    sqlite3* writerDb_ = nullptr;
    sqlite3* readerDb_ = nullptr;

    // Serializes concurrent reader access (async loads from worker threads +
    // hasChunk from main thread). Hold time <1ms per read; contention bounded
    // by ChunkPipelineSystem's async load budget.
    mutable std::mutex readerMutex_;

    // Writer prepared statements (single-thread access via ChunkSaveService batch dispatch)
    sqlite3_stmt* stmtSave_ = nullptr;

    // Reader prepared statements (guarded by readerMutex_)
    sqlite3_stmt* stmtHas_ = nullptr;
    sqlite3_stmt* stmtLoad_ = nullptr;
    sqlite3_stmt* stmtSize_ = nullptr;

    // Exact set of saved chunk coordinates. Populated at open time from DB,
    // expanded after each successful COMMIT. Guarded by shared_mutex for
    // concurrent reader access (shared_lock) and writer inserts (unique_lock).
    std::unordered_set<fabric::ChunkCoord, fabric::ChunkCoordHash> savedCoords_;
    mutable std::shared_mutex savedCoordsMutex_;
    uint32_t worldgenVersion_{0};

    void computeSavedBounds();
    void expandBounds(int cx, int cy, int cz);
};

} // namespace recurse
