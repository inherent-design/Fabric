#pragma once

#include "recurse/persistence/WorldMetadata.hh"
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace recurse {

class ChunkStore;

/// Manages the collection of worlds on disk.
/// Each world is a subdirectory under worldsDir_ containing world.toml.
class WorldRegistry {
  public:
    explicit WorldRegistry(const std::string& worldsDir);

    /// Scan worldsDir for all worlds (directories containing world.toml).
    std::vector<WorldMetadata> listWorlds() const;

    /// Create a new world directory with metadata. Returns the metadata with generated UUID.
    WorldMetadata createWorld(const std::string& name, WorldType type, int64_t seed);

    /// Delete a world directory and all its contents. Returns true if found and deleted.
    bool deleteWorld(const std::string& uuid);

    /// Look up a single world by UUID.
    std::optional<WorldMetadata> getWorld(const std::string& uuid) const;

    /// Update the lastPlayed timestamp for a world.
    void touchWorld(const std::string& uuid);

    /// Rename a world (updates display name in world.toml).
    bool renameWorld(const std::string& uuid, const std::string& newName);

    /// Open a FilesystemChunkStore for the given world UUID.
    std::unique_ptr<ChunkStore> openChunkStore(const std::string& uuid) const;

    /// Filesystem path for a world directory.
    std::string worldPath(const std::string& uuid) const;

    const std::string& worldsDir() const { return worldsDir_; }

  private:
    std::string worldsDir_;
};

} // namespace recurse
