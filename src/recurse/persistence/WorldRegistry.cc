#include "recurse/persistence/WorldRegistry.hh"

#include "fabric/utils/ErrorHandling.hh"
#include "recurse/persistence/ChunkStore.hh"
#include "recurse/persistence/FilesystemChunkStore.hh"
#include <filesystem>

namespace fs = std::filesystem;

namespace recurse {

WorldRegistry::WorldRegistry(const std::string& worldsDir) : worldsDir_(worldsDir) {
    if (!worldsDir_.empty()) {
        fs::create_directories(worldsDir_);
    }
}

std::vector<WorldMetadata> WorldRegistry::listWorlds() const {
    std::vector<WorldMetadata> worlds;
    if (!fs::is_directory(worldsDir_))
        return worlds;

    for (auto& entry : fs::directory_iterator(worldsDir_)) {
        if (!entry.is_directory())
            continue;
        auto tomlPath = entry.path() / "world.toml";
        if (!fs::exists(tomlPath))
            continue;

        try {
            worlds.push_back(WorldMetadata::fromTOML(tomlPath.string()));
        } catch (...) {
            // Skip corrupt world directories
        }
    }
    return worlds;
}

WorldMetadata WorldRegistry::createWorld(const std::string& name, WorldType type, int64_t seed) {
    WorldMetadata meta;
    meta.uuid = WorldMetadata::generateUUID();
    meta.name = name;
    meta.type = type;
    meta.seed = seed;
    meta.createdAt = WorldMetadata::nowISO8601();
    meta.lastPlayed = meta.createdAt;

    auto dir = worldPath(meta.uuid);
    fs::create_directories(dir);
    meta.toTOML(dir + "/world.toml");

    return meta;
}

bool WorldRegistry::deleteWorld(const std::string& uuid) {
    auto dir = worldPath(uuid);
    if (!fs::is_directory(dir))
        return false;
    std::error_code ec;
    fs::remove_all(dir, ec);
    return !ec;
}

std::optional<WorldMetadata> WorldRegistry::getWorld(const std::string& uuid) const {
    auto tomlPath = worldPath(uuid) + "/world.toml";
    if (!fs::exists(tomlPath))
        return std::nullopt;

    try {
        return WorldMetadata::fromTOML(tomlPath);
    } catch (...) {
        return std::nullopt;
    }
}

void WorldRegistry::touchWorld(const std::string& uuid) {
    auto tomlPath = worldPath(uuid) + "/world.toml";
    if (!fs::exists(tomlPath))
        return;

    try {
        auto meta = WorldMetadata::fromTOML(tomlPath);
        meta.lastPlayed = WorldMetadata::nowISO8601();
        meta.toTOML(tomlPath);
    } catch (...) {
        // Non-fatal; best effort
    }
}

bool WorldRegistry::renameWorld(const std::string& uuid, const std::string& newName) {
    auto tomlPath = worldPath(uuid) + "/world.toml";
    if (!fs::exists(tomlPath))
        return false;

    try {
        auto meta = WorldMetadata::fromTOML(tomlPath);
        meta.name = newName;
        meta.toTOML(tomlPath);
        return true;
    } catch (...) {
        return false;
    }
}

std::unique_ptr<ChunkStore> WorldRegistry::openChunkStore(const std::string& uuid) const {
    auto dir = worldPath(uuid);
    if (!fs::is_directory(dir)) {
        fabric::throwError("World directory not found: " + dir);
    }
    return std::make_unique<FilesystemChunkStore>(dir);
}

std::string WorldRegistry::worldPath(const std::string& uuid) const {
    return worldsDir_ + "/" + uuid;
}

} // namespace recurse
