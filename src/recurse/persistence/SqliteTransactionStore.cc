#include "recurse/persistence/SqliteTransactionStore.hh"

#include "fabric/log/Log.hh"
#include "fabric/utils/ErrorHandling.hh"
#include <chrono>
#include <cstring>
#include <sqlite3.h>
#include <string>

namespace recurse {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

namespace {

static int64_t nowMs() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

sqlite3_stmt* prepareOne(sqlite3* db, const char* sql) {
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v3(db, sql, -1, SQLITE_PREPARE_PERSISTENT, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::string msg = "sqlite3_prepare_v3 failed: ";
        msg += sqlite3_errmsg(db);
        msg += " [sql: ";
        msg += sql;
        msg += "]";
        fabric::throwError(msg);
    }
    return stmt;
}

void resetStmt(sqlite3_stmt* stmt) {
    sqlite3_reset(stmt);
    sqlite3_clear_bindings(stmt);
}

} // namespace

// ---------------------------------------------------------------------------
// Construction / Destruction
// ---------------------------------------------------------------------------

SqliteTransactionStore::SqliteTransactionStore(sqlite3* writerDb, sqlite3* readerDb)
    : writerDb_(writerDb), readerDb_(readerDb) {
    if (!writerDb_ || !readerDb_) {
        fabric::throwError("SqliteTransactionStore requires non-null database handles");
    }
    prepareStatements();
}

SqliteTransactionStore::~SqliteTransactionStore() {
    finalizeStatements();
}

// ---------------------------------------------------------------------------
// Statement lifecycle
// ---------------------------------------------------------------------------

void SqliteTransactionStore::prepareStatements() {
    stmtInsertChange_ = prepareOne(
        writerDb_, "INSERT INTO change_log (ts, cx, cy, cz, vx, vy, vz, old_cell, new_cell, player_id, source) "
                   "VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11)");

    stmtInsertSnapshot_ =
        prepareOne(writerDb_, "INSERT INTO chunk_snapshot (cx, cy, cz, ts, data) VALUES (?1, ?2, ?3, ?4, ?5)");

    stmtDeleteChanges_ = prepareOne(writerDb_, "DELETE FROM change_log WHERE ts < ?1");

    stmtDeleteSnapshots_ = prepareOne(writerDb_, "DELETE FROM chunk_snapshot WHERE ts < ?1");

    stmtLoadSnapshot_ =
        prepareOne(readerDb_, "SELECT data FROM chunk_snapshot WHERE cx=?1 AND cy=?2 AND cz=?3 AND ts < ?4 "
                              "ORDER BY ts DESC LIMIT 1");
}

void SqliteTransactionStore::finalizeStatements() {
    auto fin = [](sqlite3_stmt*& s) {
        if (s) {
            sqlite3_finalize(s);
            s = nullptr;
        }
    };
    fin(stmtInsertChange_);
    fin(stmtInsertSnapshot_);
    fin(stmtDeleteChanges_);
    fin(stmtDeleteSnapshots_);
    fin(stmtLoadSnapshot_);
}

// ---------------------------------------------------------------------------
// logChanges
// ---------------------------------------------------------------------------

void SqliteTransactionStore::logChanges(std::span<const VoxelChange> changes) {
    if (changes.empty())
        return;

    char* errMsg = nullptr;
    int rc = sqlite3_exec(writerDb_, "BEGIN IMMEDIATE", nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        FABRIC_LOG_ERROR("logChanges: BEGIN IMMEDIATE failed: {}", errMsg ? errMsg : "unknown");
        sqlite3_free(errMsg);
        return;
    }

    for (const auto& c : changes) {
        sqlite3_bind_int64(stmtInsertChange_, 1, c.timestamp);
        sqlite3_bind_int(stmtInsertChange_, 2, c.addr.cx);
        sqlite3_bind_int(stmtInsertChange_, 3, c.addr.cy);
        sqlite3_bind_int(stmtInsertChange_, 4, c.addr.cz);
        sqlite3_bind_int(stmtInsertChange_, 5, c.addr.vx);
        sqlite3_bind_int(stmtInsertChange_, 6, c.addr.vy);
        sqlite3_bind_int(stmtInsertChange_, 7, c.addr.vz);
        sqlite3_bind_int64(stmtInsertChange_, 8, static_cast<int64_t>(c.oldCell));
        sqlite3_bind_int64(stmtInsertChange_, 9, static_cast<int64_t>(c.newCell));
        sqlite3_bind_int(stmtInsertChange_, 10, c.playerId);
        sqlite3_bind_int(stmtInsertChange_, 11, static_cast<int>(c.source));

        rc = sqlite3_step(stmtInsertChange_);
        resetStmt(stmtInsertChange_);

        if (rc != SQLITE_DONE) {
            FABRIC_LOG_ERROR("logChanges: INSERT failed: {}", sqlite3_errmsg(writerDb_));
            sqlite3_exec(writerDb_, "ROLLBACK", nullptr, nullptr, nullptr);
            return;
        }
    }

    rc = sqlite3_exec(writerDb_, "COMMIT", nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        FABRIC_LOG_ERROR("logChanges: COMMIT failed: {}", errMsg ? errMsg : "unknown");
        sqlite3_free(errMsg);
        sqlite3_exec(writerDb_, "ROLLBACK", nullptr, nullptr, nullptr);
    }
}

// ---------------------------------------------------------------------------
// queryChanges (dynamic SQL)
// ---------------------------------------------------------------------------

std::vector<VoxelChange> SqliteTransactionStore::queryChanges(const ChangeQuery& query) {
    std::string sql = "SELECT ts, cx, cy, cz, vx, vy, vz, old_cell, new_cell, player_id, source "
                      "FROM change_log WHERE 1=1";

    if (query.chunkRange.has_value()) {
        const auto& [lo, hi] = *query.chunkRange;
        sql += " AND cx >= " + std::to_string(lo.x) + " AND cx <= " + std::to_string(hi.x);
        sql += " AND cy >= " + std::to_string(lo.y) + " AND cy <= " + std::to_string(hi.y);
        sql += " AND cz >= " + std::to_string(lo.z) + " AND cz <= " + std::to_string(hi.z);
    }
    if (query.fromTime > 0) {
        sql += " AND ts >= " + std::to_string(query.fromTime);
    }
    if (query.toTime < INT64_MAX) {
        sql += " AND ts <= " + std::to_string(query.toTime);
    }
    if (query.playerId != 0) {
        sql += " AND player_id = " + std::to_string(query.playerId);
    }

    sql += " ORDER BY ts DESC";
    sql += " LIMIT " + std::to_string(query.limit);
    sql += " OFFSET " + std::to_string(query.offset);

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(readerDb_, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        FABRIC_LOG_ERROR("queryChanges: prepare failed: {}", sqlite3_errmsg(readerDb_));
        return {};
    }

    std::vector<VoxelChange> results;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        VoxelChange change{};
        change.timestamp = sqlite3_column_int64(stmt, 0);
        change.addr.cx = sqlite3_column_int(stmt, 1);
        change.addr.cy = sqlite3_column_int(stmt, 2);
        change.addr.cz = sqlite3_column_int(stmt, 3);
        change.addr.vx = sqlite3_column_int(stmt, 4);
        change.addr.vy = sqlite3_column_int(stmt, 5);
        change.addr.vz = sqlite3_column_int(stmt, 6);
        change.oldCell = static_cast<uint32_t>(sqlite3_column_int64(stmt, 7));
        change.newCell = static_cast<uint32_t>(sqlite3_column_int64(stmt, 8));
        change.playerId = sqlite3_column_int(stmt, 9);
        change.source = static_cast<ChangeSource>(sqlite3_column_int(stmt, 10));
        results.push_back(change);
    }

    sqlite3_finalize(stmt);
    return results;
}

// ---------------------------------------------------------------------------
// countChanges (dynamic SQL)
// ---------------------------------------------------------------------------

int64_t SqliteTransactionStore::countChanges(const ChangeQuery& query) {
    std::string sql = "SELECT COUNT(*) FROM change_log WHERE 1=1";

    if (query.chunkRange.has_value()) {
        const auto& [lo, hi] = *query.chunkRange;
        sql += " AND cx >= " + std::to_string(lo.x) + " AND cx <= " + std::to_string(hi.x);
        sql += " AND cy >= " + std::to_string(lo.y) + " AND cy <= " + std::to_string(hi.y);
        sql += " AND cz >= " + std::to_string(lo.z) + " AND cz <= " + std::to_string(hi.z);
    }
    if (query.fromTime > 0) {
        sql += " AND ts >= " + std::to_string(query.fromTime);
    }
    if (query.toTime < INT64_MAX) {
        sql += " AND ts <= " + std::to_string(query.toTime);
    }
    if (query.playerId != 0) {
        sql += " AND player_id = " + std::to_string(query.playerId);
    }

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(readerDb_, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        FABRIC_LOG_ERROR("countChanges: prepare failed: {}", sqlite3_errmsg(readerDb_));
        return 0;
    }

    int64_t count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        count = sqlite3_column_int64(stmt, 0);
    }

    sqlite3_finalize(stmt);
    return count;
}

// ---------------------------------------------------------------------------
// saveSnapshot
// ---------------------------------------------------------------------------

void SqliteTransactionStore::saveSnapshot(int cx, int cy, int cz, const ChunkBlob& data) {
    int64_t ts = nowMs();

    sqlite3_bind_int(stmtInsertSnapshot_, 1, cx);
    sqlite3_bind_int(stmtInsertSnapshot_, 2, cy);
    sqlite3_bind_int(stmtInsertSnapshot_, 3, cz);
    sqlite3_bind_int64(stmtInsertSnapshot_, 4, ts);
    sqlite3_bind_blob(stmtInsertSnapshot_, 5, data.data_ptr(), static_cast<int>(data.size()), SQLITE_STATIC);

    int rc = sqlite3_step(stmtInsertSnapshot_);
    resetStmt(stmtInsertSnapshot_);

    if (rc != SQLITE_DONE) {
        FABRIC_LOG_ERROR("saveSnapshot({},{},{}) failed: {}", cx, cy, cz, sqlite3_errmsg(writerDb_));
    }
}

// ---------------------------------------------------------------------------
// loadSnapshot
// ---------------------------------------------------------------------------

std::optional<ChunkBlob> SqliteTransactionStore::loadSnapshot(int cx, int cy, int cz, int64_t beforeTime) {
    sqlite3_bind_int(stmtLoadSnapshot_, 1, cx);
    sqlite3_bind_int(stmtLoadSnapshot_, 2, cy);
    sqlite3_bind_int(stmtLoadSnapshot_, 3, cz);
    sqlite3_bind_int64(stmtLoadSnapshot_, 4, beforeTime);

    std::optional<ChunkBlob> result;
    if (sqlite3_step(stmtLoadSnapshot_) == SQLITE_ROW) {
        const void* blobData = sqlite3_column_blob(stmtLoadSnapshot_, 0);
        int blobSize = sqlite3_column_bytes(stmtLoadSnapshot_, 0);
        if (blobData && blobSize > 0) {
            ChunkBlob blob(static_cast<size_t>(blobSize));
            std::memcpy(blob.data_ptr(), blobData, static_cast<size_t>(blobSize));
            result = std::move(blob);
        }
    }

    resetStmt(stmtLoadSnapshot_);
    return result;
}

// ---------------------------------------------------------------------------
// rollback
// ---------------------------------------------------------------------------

std::vector<fabric::ChunkCoord> SqliteTransactionStore::rollback(const RollbackSpec& spec) {
    const auto& [lo, hi] = spec.chunkRange;

    std::string sql = "SELECT DISTINCT cx, cy, cz FROM change_log "
                      "WHERE cx >= " +
                      std::to_string(lo.x) + " AND cx <= " + std::to_string(hi.x) +
                      " AND cy >= " + std::to_string(lo.y) + " AND cy <= " + std::to_string(hi.y) +
                      " AND cz >= " + std::to_string(lo.z) + " AND cz <= " + std::to_string(hi.z) +
                      " AND ts >= " + std::to_string(spec.targetTime);

    if (spec.playerId != 0) {
        sql += " AND player_id = " + std::to_string(spec.playerId);
    }

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(readerDb_, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        FABRIC_LOG_ERROR("rollback: prepare failed: {}", sqlite3_errmsg(readerDb_));
        return {};
    }

    std::vector<fabric::ChunkCoord> affected;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        fabric::ChunkCoord coord{};
        coord.x = sqlite3_column_int(stmt, 0);
        coord.y = sqlite3_column_int(stmt, 1);
        coord.z = sqlite3_column_int(stmt, 2);
        affected.push_back(coord);
    }

    sqlite3_finalize(stmt);

    FABRIC_LOG_WARN("rollback: returning {} affected chunks; cell-level reverse-apply not yet implemented (WT-5)",
                    affected.size());
    return affected;
}

// ---------------------------------------------------------------------------
// prune
// ---------------------------------------------------------------------------

void SqliteTransactionStore::prune(int64_t retainChangesAfter, int64_t retainSnapshotsAfter) {
    char* errMsg = nullptr;
    int rc = sqlite3_exec(writerDb_, "BEGIN IMMEDIATE", nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        FABRIC_LOG_ERROR("prune: BEGIN IMMEDIATE failed: {}", errMsg ? errMsg : "unknown");
        sqlite3_free(errMsg);
        return;
    }

    sqlite3_bind_int64(stmtDeleteChanges_, 1, retainChangesAfter);
    rc = sqlite3_step(stmtDeleteChanges_);
    resetStmt(stmtDeleteChanges_);

    if (rc != SQLITE_DONE) {
        FABRIC_LOG_ERROR("prune: DELETE change_log failed: {}", sqlite3_errmsg(writerDb_));
        sqlite3_exec(writerDb_, "ROLLBACK", nullptr, nullptr, nullptr);
        return;
    }

    sqlite3_bind_int64(stmtDeleteSnapshots_, 1, retainSnapshotsAfter);
    rc = sqlite3_step(stmtDeleteSnapshots_);
    resetStmt(stmtDeleteSnapshots_);

    if (rc != SQLITE_DONE) {
        FABRIC_LOG_ERROR("prune: DELETE chunk_snapshot failed: {}", sqlite3_errmsg(writerDb_));
        sqlite3_exec(writerDb_, "ROLLBACK", nullptr, nullptr, nullptr);
        return;
    }

    rc = sqlite3_exec(writerDb_, "COMMIT", nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        FABRIC_LOG_ERROR("prune: COMMIT failed: {}", errMsg ? errMsg : "unknown");
        sqlite3_free(errMsg);
        sqlite3_exec(writerDb_, "ROLLBACK", nullptr, nullptr, nullptr);
    }
}

// ---------------------------------------------------------------------------
// flush
// ---------------------------------------------------------------------------

void SqliteTransactionStore::flush() {
    // No-op. All operations are currently synchronous.
    // Reserved for future batching.
}

} // namespace recurse
