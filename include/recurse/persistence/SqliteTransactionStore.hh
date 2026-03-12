#pragma once

#include "recurse/persistence/WorldTransactionStore.hh"
#include <string>

struct sqlite3;
struct sqlite3_stmt;

namespace recurse {

/// SQLite-backed WorldTransactionStore. Operates on change_log and chunk_snapshot tables.
///
/// Takes raw sqlite3 handles from SqliteChunkStore; does not own them.
/// Writer handle for inserts/deletes, reader handle for queries.
class SqliteTransactionStore : public WorldTransactionStore {
  public:
    /// Attach to an existing SQLite database. Takes raw handles; does not own them.
    SqliteTransactionStore(sqlite3* writerDb, sqlite3* readerDb);
    ~SqliteTransactionStore() override;

    SqliteTransactionStore(const SqliteTransactionStore&) = delete;
    SqliteTransactionStore& operator=(const SqliteTransactionStore&) = delete;

    void logChanges(std::span<const VoxelChange> changes) override;
    std::vector<VoxelChange> queryChanges(const ChangeQuery& query) override;
    int64_t countChanges(const ChangeQuery& query) override;
    void saveSnapshot(int cx, int cy, int cz, const ChunkBlob& data) override;
    std::optional<ChunkBlob> loadSnapshot(int cx, int cy, int cz, int64_t beforeTime) override;
    std::vector<fabric::ChunkCoord> rollback(const RollbackSpec& spec) override;
    void prune(int64_t retainChangesAfter, int64_t retainSnapshotsAfter) override;
    void flush() override;

  private:
    void prepareStatements();
    void finalizeStatements();

    sqlite3* writerDb_;
    sqlite3* readerDb_;

    // Writer statements
    sqlite3_stmt* stmtInsertChange_ = nullptr;
    sqlite3_stmt* stmtInsertSnapshot_ = nullptr;
    sqlite3_stmt* stmtDeleteChanges_ = nullptr;
    sqlite3_stmt* stmtDeleteSnapshots_ = nullptr;

    // Reader statements
    sqlite3_stmt* stmtLoadSnapshot_ = nullptr;
};

} // namespace recurse
