#include "recurse/persistence/WorldMetadata.hh"

#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>

namespace fs = std::filesystem;

class WorldMetadataTest : public ::testing::Test {
  protected:
    void SetUp() override {
        tmpDir_ = fs::temp_directory_path() / "fabric_test_metadata";
        fs::create_directories(tmpDir_);
    }
    void TearDown() override { fs::remove_all(tmpDir_); }

    fs::path tmpDir_;
};

TEST_F(WorldMetadataTest, GenerateUUIDIsEightHexChars) {
    auto uuid = recurse::WorldMetadata::generateUUID();
    EXPECT_EQ(uuid.size(), 8u);
    for (char c : uuid) {
        EXPECT_TRUE((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')) << "Non-hex char: " << c;
    }
}

TEST_F(WorldMetadataTest, GenerateUUIDIsUnique) {
    auto a = recurse::WorldMetadata::generateUUID();
    auto b = recurse::WorldMetadata::generateUUID();
    EXPECT_NE(a, b);
}

TEST_F(WorldMetadataTest, NowISO8601HasCorrectFormat) {
    auto ts = recurse::WorldMetadata::nowISO8601();
    // Format: YYYY-MM-DDTHH:MM:SSZ
    EXPECT_EQ(ts.size(), 20u);
    EXPECT_EQ(ts[4], '-');
    EXPECT_EQ(ts[7], '-');
    EXPECT_EQ(ts[10], 'T');
    EXPECT_EQ(ts[13], ':');
    EXPECT_EQ(ts[16], ':');
    EXPECT_EQ(ts[19], 'Z');
}

TEST_F(WorldMetadataTest, RoundTripTOML) {
    recurse::WorldMetadata original;
    original.uuid = "abcd1234";
    original.name = "Test World";
    original.type = recurse::WorldType::Natural;
    original.seed = 42;
    original.createdAt = "2026-03-09T12:00:00Z";
    original.lastPlayed = "2026-03-09T13:00:00Z";

    auto path = (tmpDir_ / "world.toml").string();
    original.toTOML(path);

    auto loaded = recurse::WorldMetadata::fromTOML(path);
    EXPECT_EQ(loaded.uuid, original.uuid);
    EXPECT_EQ(loaded.name, original.name);
    EXPECT_EQ(loaded.type, original.type);
    EXPECT_EQ(loaded.seed, original.seed);
    EXPECT_EQ(loaded.createdAt, original.createdAt);
    EXPECT_EQ(loaded.lastPlayed, original.lastPlayed);
}

TEST_F(WorldMetadataTest, RoundTripFlatWorld) {
    recurse::WorldMetadata original;
    original.uuid = "flat0001";
    original.name = "Flat Test";
    original.type = recurse::WorldType::Flat;
    original.seed = -1;
    original.createdAt = "2026-01-01T00:00:00Z";
    original.lastPlayed = "2026-01-01T00:00:00Z";

    auto path = (tmpDir_ / "world.toml").string();
    original.toTOML(path);

    auto loaded = recurse::WorldMetadata::fromTOML(path);
    EXPECT_EQ(loaded.type, recurse::WorldType::Flat);
    EXPECT_EQ(loaded.seed, -1);
}

TEST_F(WorldMetadataTest, FromTOMLThrowsOnMissingUUID) {
    auto path = (tmpDir_ / "bad.toml").string();
    {
        std::ofstream out(path);
        out << "name = \"NoUUID\"\ntype = \"Flat\"\nseed = 0\n";
    }
    EXPECT_THROW(recurse::WorldMetadata::fromTOML(path), std::exception);
}
