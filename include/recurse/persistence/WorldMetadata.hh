#pragma once

#include "recurse/world/WorldType.hh"
#include <cmath>
#include <cstdint>
#include <string>

namespace recurse {

/// Per-world metadata stored as world.toml in the world directory.
struct WorldMetadata {
    std::string uuid; // 8-char hex ID (generated on creation)
    std::string name; // user-visible display name
    WorldType type{WorldType::Flat};
    int64_t seed{0};        // terrain generation seed
    std::string createdAt;  // ISO 8601 timestamp
    std::string lastPlayed; // ISO 8601 timestamp

    // Player state. NaN = not set (use default spawn).
    float playerX{0.0f / 0.0f};
    float playerY{0.0f / 0.0f};
    float playerZ{0.0f / 0.0f};
    bool hasPlayerPosition() const;

    /// Read metadata from a world.toml file. Throws FabricException on parse failure.
    static WorldMetadata fromTOML(const std::string& path);

    /// Write metadata to a world.toml file.
    void toTOML(const std::string& path) const;

    /// Generate a random 8-char hex UUID.
    static std::string generateUUID();

    /// Current time as ISO 8601 string.
    static std::string nowISO8601();
};

} // namespace recurse
