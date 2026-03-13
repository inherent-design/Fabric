#include "recurse/persistence/SqliteChunkStore.hh"

#include "fabric/core/Log.hh"
#include "fabric/utils/ErrorHandling.hh"
#include "recurse/persistence/SchemaMigrations.hh"
#include <algorithm>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <sqlite3.h>
#include <string>

namespace fs = std::filesystem;

namespace recurse {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

namespace {

/// Prepare a single persistent statement on the given connection.
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

/// Reset and clear bindings on a prepared statement.
void resetStmt(sqlite3_stmt* stmt) {
    sqlite3_reset(stmt);
    sqlite3_clear_bindings(stmt);
}

} // namespace

// ---------------------------------------------------------------------------
// Construction / Destruction
// ---------------------------------------------------------------------------

SqliteChunkStore::SqliteChunkStore(const std::string& worldDir) {
    dbPath_ = worldDir + "/world.db";
    fs::create_directories(worldDir);

    // Leftover WAL indicates a previous dirty shutdown.
    if (fs::exists(dbPath_ + "-wal")) {
        FABRIC_LOG_WARN("world.db-wal exists at open; previous session may not have shut down cleanly");
    }

    openConnections(dbPath_);
}

SqliteChunkStore::~SqliteChunkStore() {
    close();
}

// ---------------------------------------------------------------------------
// Connection lifecycle
// ---------------------------------------------------------------------------

void SqliteChunkStore::openConnections(const std::string& dbPath) {
    // Writer connection
    int rc = sqlite3_open(dbPath.c_str(), &writerDb_);
    if (rc != SQLITE_OK) {
        std::string msg = "Failed to open writer DB: ";
        msg += sqlite3_errmsg(writerDb_);
        sqlite3_close(writerDb_);
        writerDb_ = nullptr;
        fabric::throwError(msg);
    }

    configurePragmas(writerDb_);
    runMigrations();

    // Reader connection
    rc = sqlite3_open_v2(dbPath.c_str(), &readerDb_, SQLITE_OPEN_READONLY, nullptr);
    if (rc != SQLITE_OK) {
        std::string msg = "Failed to open reader DB: ";
        msg += sqlite3_errmsg(readerDb_);
        sqlite3_close(readerDb_);
        readerDb_ = nullptr;
        fabric::throwError(msg);
    }
    configurePragmas(readerDb_);

    prepareStatements();
    computeSavedBounds();
}

void SqliteChunkStore::configurePragmas(sqlite3* db) {
    execSql(db, "PRAGMA journal_mode = WAL");
    execSql(db, "PRAGMA synchronous = NORMAL");
    execSql(db, "PRAGMA page_size = 16384");
    execSql(db, "PRAGMA wal_autocheckpoint = 0");
    execSql(db, "PRAGMA cache_size = -64000");
    execSql(db, "PRAGMA mmap_size = 268435456");
    execSql(db, "PRAGMA temp_store = MEMORY");
    execSql(db, "PRAGMA foreign_keys = OFF");
    execSql(db, "PRAGMA busy_timeout = 100");
}

void SqliteChunkStore::runMigrations() {
    // Read current schema version.
    sqlite3_stmt* versionStmt = nullptr;
    int rc = sqlite3_prepare_v2(writerDb_, "PRAGMA user_version", -1, &versionStmt, nullptr);
    if (rc != SQLITE_OK) {
        fabric::throwError("Failed to query user_version: " + std::string(sqlite3_errmsg(writerDb_)));
    }

    int currentVersion = 0;
    if (sqlite3_step(versionStmt) == SQLITE_ROW) {
        currentVersion = sqlite3_column_int(versionStmt, 0);
    }
    sqlite3_finalize(versionStmt);

    const int targetVersion = persistence::K_SCHEMA_VERSION;

    if (currentVersion > targetVersion) {
        fabric::throwError("world.db schema version " + std::to_string(currentVersion) +
                           " is newer than this build supports (max " + std::to_string(targetVersion) +
                           "); update Fabric to load this world");
    }

    if (currentVersion == targetVersion)
        return;

    // Apply each migration in its own transaction.
    for (int i = currentVersion; i < targetVersion; ++i) {
        const auto& migration = persistence::K_SCHEMA_MIGRATIONS[static_cast<size_t>(i)];
        int newVersion = i + 1;

        execSql(writerDb_, "BEGIN IMMEDIATE");

        // Execute migration SQL (may contain multiple statements).
        char* errMsg = nullptr;
        rc = sqlite3_exec(writerDb_, migration.sql.data(), nullptr, nullptr, &errMsg);
        if (rc != SQLITE_OK) {
            std::string msg = "Migration " + std::to_string(newVersion) + " failed: ";
            if (errMsg) {
                msg += errMsg;
                sqlite3_free(errMsg);
            }
            sqlite3_exec(writerDb_, "ROLLBACK", nullptr, nullptr, nullptr);
            fabric::throwError(msg);
        }

        // Record in audit table. The table was just created by migration 1,
        // so this INSERT is safe for all migrations including the first.
        std::string auditSql = "INSERT INTO schema_version (version, applied_at, description) VALUES (" +
                               std::to_string(newVersion) + ", " + std::to_string(std::time(nullptr)) + ", '" +
                               std::string(migration.description) + "')";
        execSql(writerDb_, auditSql.c_str());

        // Set authoritative version.
        std::string pragmaSql = "PRAGMA user_version = " + std::to_string(newVersion);
        execSql(writerDb_, pragmaSql.c_str());

        execSql(writerDb_, "COMMIT");
    }
}

void SqliteChunkStore::prepareStatements() {
    stmtHas_ = prepareOne(readerDb_, "SELECT 1 FROM chunk_state WHERE cx=?1 AND cy=?2 AND cz=?3");
    stmtLoad_ = prepareOne(readerDb_, "SELECT data FROM chunk_state WHERE cx=?1 AND cy=?2 AND cz=?3");
    stmtSize_ = prepareOne(readerDb_, "SELECT length(data) FROM chunk_state WHERE cx=?1 AND cy=?2 AND cz=?3");
    stmtSave_ = prepareOne(
        writerDb_, "INSERT OR REPLACE INTO chunk_state (cx, cy, cz, data, updated_at) VALUES (?1, ?2, ?3, ?4, ?5)");
}

void SqliteChunkStore::finalizeStatements() {
    if (stmtHas_) {
        sqlite3_finalize(stmtHas_);
        stmtHas_ = nullptr;
    }
    if (stmtLoad_) {
        sqlite3_finalize(stmtLoad_);
        stmtLoad_ = nullptr;
    }
    if (stmtSize_) {
        sqlite3_finalize(stmtSize_);
        stmtSize_ = nullptr;
    }
    if (stmtSave_) {
        sqlite3_finalize(stmtSave_);
        stmtSave_ = nullptr;
    }
}

void SqliteChunkStore::execSql(sqlite3* db, const char* sql) {
    char* errMsg = nullptr;
    int rc = sqlite3_exec(db, sql, nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        std::string msg = "sqlite3_exec failed: ";
        if (errMsg) {
            msg += errMsg;
            sqlite3_free(errMsg);
        }
        msg += " [sql: ";
        msg += sql;
        msg += "]";
        fabric::throwError(msg);
    }
}

// ---------------------------------------------------------------------------
// Saved-region bounding box
// ---------------------------------------------------------------------------

void SqliteChunkStore::computeSavedBounds() {
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(
        readerDb_, "SELECT MIN(cx), MAX(cx), MIN(cy), MAX(cy), MIN(cz), MAX(cz) FROM chunk_state", -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        FABRIC_LOG_WARN("computeSavedBounds: prepare failed: {}", sqlite3_errmsg(readerDb_));
        return;
    }

    if (sqlite3_step(stmt) == SQLITE_ROW && sqlite3_column_type(stmt, 0) != SQLITE_NULL) {
        savedBounds_.minCx = sqlite3_column_int(stmt, 0);
        savedBounds_.maxCx = sqlite3_column_int(stmt, 1);
        savedBounds_.minCy = sqlite3_column_int(stmt, 2);
        savedBounds_.maxCy = sqlite3_column_int(stmt, 3);
        savedBounds_.minCz = sqlite3_column_int(stmt, 4);
        savedBounds_.maxCz = sqlite3_column_int(stmt, 5);
        savedBounds_.empty = false;
    }

    sqlite3_finalize(stmt);
}

bool SqliteChunkStore::isInSavedRegion(int cx, int cy, int cz) const {
    if (savedBounds_.empty)
        return false;
    return cx >= savedBounds_.minCx && cx <= savedBounds_.maxCx && cy >= savedBounds_.minCy &&
           cy <= savedBounds_.maxCy && cz >= savedBounds_.minCz && cz <= savedBounds_.maxCz;
}

// ---------------------------------------------------------------------------
// ChunkStore v2 API
// ---------------------------------------------------------------------------

bool SqliteChunkStore::hasChunk(int cx, int cy, int cz) const {
    std::lock_guard lock(readerMutex_);
    sqlite3_bind_int(stmtHas_, 1, cx);
    sqlite3_bind_int(stmtHas_, 2, cy);
    sqlite3_bind_int(stmtHas_, 3, cz);

    bool found = (sqlite3_step(stmtHas_) == SQLITE_ROW);
    resetStmt(stmtHas_);
    return found;
}

std::optional<ChunkBlob> SqliteChunkStore::loadChunk(int cx, int cy, int cz) const {
    std::lock_guard lock(readerMutex_);
    sqlite3_bind_int(stmtLoad_, 1, cx);
    sqlite3_bind_int(stmtLoad_, 2, cy);
    sqlite3_bind_int(stmtLoad_, 3, cz);

    std::optional<ChunkBlob> result;
    if (sqlite3_step(stmtLoad_) == SQLITE_ROW) {
        const void* blobData = sqlite3_column_blob(stmtLoad_, 0);
        int blobSize = sqlite3_column_bytes(stmtLoad_, 0);
        if (blobData && blobSize > 0) {
            ChunkBlob blob(static_cast<size_t>(blobSize));
            std::memcpy(blob.data_ptr(), blobData, static_cast<size_t>(blobSize));
            result = std::move(blob);
        }
    }
    resetStmt(stmtLoad_);
    return result;
}

void SqliteChunkStore::saveChunk(int cx, int cy, int cz, const ChunkBlob& data) {
    if (data.empty())
        return;

    sqlite3_bind_int(stmtSave_, 1, cx);
    sqlite3_bind_int(stmtSave_, 2, cy);
    sqlite3_bind_int(stmtSave_, 3, cz);
    sqlite3_bind_blob(stmtSave_, 4, data.data_ptr(), static_cast<int>(data.size()), SQLITE_STATIC);
    sqlite3_bind_int64(stmtSave_, 5, static_cast<sqlite3_int64>(std::time(nullptr)));

    int rc = sqlite3_step(stmtSave_);
    resetStmt(stmtSave_);

    if (rc != SQLITE_DONE) {
        FABRIC_LOG_ERROR("saveChunk({},{},{}) failed: {}", cx, cy, cz, sqlite3_errmsg(writerDb_));
    }
}

size_t SqliteChunkStore::chunkSize(int cx, int cy, int cz) const {
    std::lock_guard lock(readerMutex_);
    sqlite3_bind_int(stmtSize_, 1, cx);
    sqlite3_bind_int(stmtSize_, 2, cy);
    sqlite3_bind_int(stmtSize_, 3, cz);

    size_t result = 0;
    if (sqlite3_step(stmtSize_) == SQLITE_ROW) {
        result = static_cast<size_t>(sqlite3_column_int64(stmtSize_, 0));
    }
    resetStmt(stmtSize_);
    return result;
}

// ---------------------------------------------------------------------------
// Batch operations
// ---------------------------------------------------------------------------

std::vector<std::pair<fabric::ChunkCoord, ChunkBlob>>
SqliteChunkStore::loadBatch(const std::vector<fabric::ChunkCoord>& coords) const {
    auto sorted = coords;
    std::sort(sorted.begin(), sorted.end(), [](const fabric::ChunkCoord& a, const fabric::ChunkCoord& b) {
        return std::tie(a.x, a.y, a.z) < std::tie(b.x, b.y, b.z);
    });

    std::vector<std::pair<fabric::ChunkCoord, ChunkBlob>> results;
    results.reserve(sorted.size());

    for (const auto& coord : sorted) {
        auto blob = loadChunk(coord.x, coord.y, coord.z);
        if (blob)
            results.push_back({coord, std::move(*blob)});
    }
    return results;
}

void SqliteChunkStore::saveBatch(const std::vector<std::pair<fabric::ChunkCoord, ChunkBlob>>& entries) {
    if (entries.empty())
        return;

    execSql(writerDb_, "BEGIN IMMEDIATE");

    for (const auto& [coord, blob] : entries) {
        sqlite3_bind_int(stmtSave_, 1, coord.x);
        sqlite3_bind_int(stmtSave_, 2, coord.y);
        sqlite3_bind_int(stmtSave_, 3, coord.z);
        sqlite3_bind_blob(stmtSave_, 4, blob.data_ptr(), static_cast<int>(blob.size()), SQLITE_STATIC);
        sqlite3_bind_int64(stmtSave_, 5, static_cast<sqlite3_int64>(std::time(nullptr)));

        int rc = sqlite3_step(stmtSave_);
        resetStmt(stmtSave_);

        if (rc != SQLITE_DONE) {
            FABRIC_LOG_ERROR("saveBatch: chunk ({},{},{}) failed: {}", coord.x, coord.y, coord.z,
                             sqlite3_errmsg(writerDb_));
            sqlite3_exec(writerDb_, "ROLLBACK", nullptr, nullptr, nullptr);
            return;
        }
    }

    execSql(writerDb_, "COMMIT");
}

// ---------------------------------------------------------------------------
// WAL checkpoint
// ---------------------------------------------------------------------------

void SqliteChunkStore::maybeCheckpoint() {
    if (!writerDb_)
        return;

    int nLog = 0;
    int nCkpt = 0;
    sqlite3_wal_checkpoint_v2(writerDb_, nullptr, SQLITE_CHECKPOINT_PASSIVE, &nLog, &nCkpt);
}

// ---------------------------------------------------------------------------
// Shutdown
// ---------------------------------------------------------------------------

void SqliteChunkStore::close() {
    finalizeStatements();

    if (readerDb_) {
        sqlite3_close(readerDb_);
        readerDb_ = nullptr;
    }

    if (writerDb_) {
        // TRUNCATE checkpoint removes WAL/SHM files.
        int nLog = 0;
        int nCkpt = 0;
        sqlite3_wal_checkpoint_v2(writerDb_, nullptr, SQLITE_CHECKPOINT_TRUNCATE, &nLog, &nCkpt);

        sqlite3_exec(writerDb_, "PRAGMA optimize", nullptr, nullptr, nullptr);
        sqlite3_close(writerDb_);
        writerDb_ = nullptr;
    }
}

} // namespace recurse
