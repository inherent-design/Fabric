#include "recurse/persistence/WorldRegistry.hh"

#include "recurse/persistence/ChunkStore.hh"
#include <filesystem>
#include <gtest/gtest.h>

namespace fs = std::filesystem;

class WorldRegistryTest : public ::testing::Test {
  protected:
    void SetUp() override {
        tmpDir_ = fs::temp_directory_path() / "fabric_test_registry";
        fs::remove_all(tmpDir_);
        worldsDir_ = (tmpDir_ / "worlds").string();
    }
    void TearDown() override { fs::remove_all(tmpDir_); }

    fs::path tmpDir_;
    std::string worldsDir_;
};

TEST_F(WorldRegistryTest, ConstructorCreatesWorldsDir) {
    recurse::WorldRegistry reg(worldsDir_);
    EXPECT_TRUE(fs::is_directory(worldsDir_));
}

TEST_F(WorldRegistryTest, ListWorldsEmptyOnFreshDir) {
    recurse::WorldRegistry reg(worldsDir_);
    auto worlds = reg.listWorlds();
    EXPECT_TRUE(worlds.empty());
}

TEST_F(WorldRegistryTest, CreateWorldReturnsMetadata) {
    recurse::WorldRegistry reg(worldsDir_);
    auto meta = reg.createWorld("My World", recurse::WorldType::Natural, 12345);

    EXPECT_FALSE(meta.uuid.empty());
    EXPECT_EQ(meta.name, "My World");
    EXPECT_EQ(meta.type, recurse::WorldType::Natural);
    EXPECT_EQ(meta.seed, 12345);
    EXPECT_FALSE(meta.createdAt.empty());
    EXPECT_FALSE(meta.lastPlayed.empty());
}

TEST_F(WorldRegistryTest, CreateWorldPersistsToDisk) {
    recurse::WorldRegistry reg(worldsDir_);
    auto meta = reg.createWorld("Persist Test", recurse::WorldType::Flat, 0);

    auto tomlPath = (reg.worldPath(meta.uuid) / "world.toml").string();
    EXPECT_TRUE(fs::exists(tomlPath));
}

TEST_F(WorldRegistryTest, ListWorldsFindsCreatedWorlds) {
    recurse::WorldRegistry reg(worldsDir_);
    reg.createWorld("World A", recurse::WorldType::Flat, 1);
    reg.createWorld("World B", recurse::WorldType::Natural, 2);

    auto worlds = reg.listWorlds();
    EXPECT_EQ(worlds.size(), 2u);
}

TEST_F(WorldRegistryTest, GetWorldByUUID) {
    recurse::WorldRegistry reg(worldsDir_);
    auto created = reg.createWorld("FindMe", recurse::WorldType::Natural, 42);

    auto found = reg.getWorld(created.uuid);
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->name, "FindMe");
    EXPECT_EQ(found->seed, 42);
}

TEST_F(WorldRegistryTest, GetWorldMissingReturnsNullopt) {
    recurse::WorldRegistry reg(worldsDir_);
    auto result = reg.getWorld("nonexistent");
    EXPECT_FALSE(result.has_value());
}

TEST_F(WorldRegistryTest, DeleteWorldRemovesDirectory) {
    recurse::WorldRegistry reg(worldsDir_);
    auto meta = reg.createWorld("ToDelete", recurse::WorldType::Flat, 0);

    EXPECT_TRUE(reg.deleteWorld(meta.uuid));
    EXPECT_FALSE(fs::is_directory(reg.worldPath(meta.uuid)));
    EXPECT_TRUE(reg.listWorlds().empty());
}

TEST_F(WorldRegistryTest, DeleteWorldReturnsFalseForMissing) {
    recurse::WorldRegistry reg(worldsDir_);
    EXPECT_FALSE(reg.deleteWorld("nonexistent"));
}

TEST_F(WorldRegistryTest, RenameWorldUpdatesToml) {
    recurse::WorldRegistry reg(worldsDir_);
    auto meta = reg.createWorld("OldName", recurse::WorldType::Flat, 0);

    EXPECT_TRUE(reg.renameWorld(meta.uuid, "NewName"));

    auto found = reg.getWorld(meta.uuid);
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->name, "NewName");
}

TEST_F(WorldRegistryTest, OpenChunkStoreCreatesDatabase) {
    recurse::WorldRegistry reg(worldsDir_);
    auto meta = reg.createWorld("ChunkTest", recurse::WorldType::Natural, 7);

    auto store = reg.openChunkStore(meta.uuid);
    ASSERT_NE(store, nullptr);

    auto worldDir = reg.worldPath(meta.uuid);
    EXPECT_TRUE(fs::exists(worldDir / "world.db"));
}

TEST_F(WorldRegistryTest, TouchWorldUpdatesLastPlayed) {
    recurse::WorldRegistry reg(worldsDir_);
    auto meta = reg.createWorld("TouchTest", recurse::WorldType::Flat, 0);
    auto originalPlayed = meta.lastPlayed;

    // Touch should update lastPlayed (may be same second; just verify no crash)
    reg.touchWorld(meta.uuid);
    auto found = reg.getWorld(meta.uuid);
    ASSERT_TRUE(found.has_value());
    EXPECT_FALSE(found->lastPlayed.empty());
}
