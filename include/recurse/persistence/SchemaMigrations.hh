#pragma once

#include <array>
#include <cstdint>
#include <string_view>

namespace recurse::persistence {

struct Migration {
    std::string_view sql;
    std::string_view description;
};

/// Append-only migration list. Never remove or reorder entries.
/// PRAGMA user_version is the authoritative version for dispatch.
/// The schema_version table is an audit log only.
inline constexpr std::array K_SCHEMA_MIGRATIONS = {
    Migration{
        R"sql(
CREATE TABLE chunk_state (
    id INTEGER PRIMARY KEY,
    cx INTEGER NOT NULL,
    cy INTEGER NOT NULL,
    cz INTEGER NOT NULL,
    data BLOB NOT NULL,
    updated_at INTEGER NOT NULL,
    UNIQUE (cx, cy, cz)
);

CREATE TABLE change_log (
    id INTEGER PRIMARY KEY,
    ts INTEGER NOT NULL,
    cx INTEGER NOT NULL,
    cy INTEGER NOT NULL,
    cz INTEGER NOT NULL,
    vx INTEGER NOT NULL,
    vy INTEGER NOT NULL,
    vz INTEGER NOT NULL,
    old_cell INTEGER NOT NULL,
    new_cell INTEGER NOT NULL,
    player_id INTEGER NOT NULL,
    source INTEGER NOT NULL
);

CREATE INDEX idx_changelog_chunk_time ON change_log (cx, cy, cz, ts);
CREATE INDEX idx_changelog_player_time ON change_log (player_id, ts);

CREATE TABLE chunk_snapshot (
    id INTEGER PRIMARY KEY,
    cx INTEGER NOT NULL,
    cy INTEGER NOT NULL,
    cz INTEGER NOT NULL,
    ts INTEGER NOT NULL,
    data BLOB NOT NULL,
    UNIQUE (cx, cy, cz, ts)
);

CREATE INDEX idx_snapshot_chunk_time ON chunk_snapshot (cx, cy, cz, ts);

CREATE TABLE schema_version (
    version INTEGER PRIMARY KEY,
    applied_at INTEGER NOT NULL,
    description TEXT NOT NULL
);
)sql",
        "initial schema: chunk_state, change_log, chunk_snapshot, schema_version"},
};

inline constexpr int K_SCHEMA_VERSION = static_cast<int>(K_SCHEMA_MIGRATIONS.size());

} // namespace recurse::persistence
