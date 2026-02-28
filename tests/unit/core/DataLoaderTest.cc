#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

#include "fabric/core/DataLoader.hh"

namespace fabric {

class DataLoaderTest : public ::testing::Test {
  protected:
    // Write a temp TOML file for file-based tests
    std::filesystem::path writeTempFile(const std::string& content, const std::string& name = "test.toml") {
        auto dir = std::filesystem::temp_directory_path() / "fabric_test";
        std::filesystem::create_directories(dir);
        auto path = dir / name;
        std::ofstream ofs(path);
        ofs << content;
        ofs.close();
        return path;
    }

    void TearDown() override {
        auto dir = std::filesystem::temp_directory_path() / "fabric_test";
        std::filesystem::remove_all(dir);
    }
};

// -- DataLoader::parse --

TEST_F(DataLoaderTest, ParseValidToml) {
    auto result = DataLoader::parse(R"(
        [player]
        name = "Ada"
        level = 42
        speed = 3.14
        active = true
    )");
    ASSERT_TRUE(result.isOk());
    auto& loader = result.value();

    EXPECT_EQ(loader.getString("player.name").value(), "Ada");
    EXPECT_EQ(loader.getInt("player.level").value(), 42);
    EXPECT_DOUBLE_EQ(loader.getFloat("player.speed").value(), 3.14);
    EXPECT_TRUE(loader.getBool("player.active").value());
}

TEST_F(DataLoaderTest, ParseMalformedToml) {
    auto result = DataLoader::parse("[invalid\nno_closing_bracket");
    ASSERT_TRUE(result.isError());
    EXPECT_EQ(result.code(), ErrorCode::Internal);
    // Error message should contain line info
    EXPECT_NE(result.message().find(":"), std::string::npos);
}

TEST_F(DataLoaderTest, MissingKey) {
    auto result = DataLoader::parse("[section]\nkey = 1");
    ASSERT_TRUE(result.isOk());
    auto& loader = result.value();

    auto missing = loader.getString("section.nonexistent");
    EXPECT_TRUE(missing.isError());
    EXPECT_EQ(missing.code(), ErrorCode::NotFound);
}

TEST_F(DataLoaderTest, TypeMismatch) {
    auto result = DataLoader::parse("[data]\nvalue = \"text\"");
    ASSERT_TRUE(result.isOk());
    auto& loader = result.value();

    auto asInt = loader.getInt("data.value");
    EXPECT_TRUE(asInt.isError());
    EXPECT_EQ(asInt.code(), ErrorCode::InvalidState);
}

TEST_F(DataLoaderTest, HasKey) {
    auto result = DataLoader::parse("[a]\nb = 1");
    ASSERT_TRUE(result.isOk());
    auto& loader = result.value();

    EXPECT_TRUE(loader.hasKey("a.b"));
    EXPECT_TRUE(loader.hasKey("a"));
    EXPECT_FALSE(loader.hasKey("a.c"));
    EXPECT_FALSE(loader.hasKey("z"));
}

TEST_F(DataLoaderTest, StringArray) {
    auto result = DataLoader::parse(R"(
        [item]
        tags = ["fire", "rare", "quest"]
    )");
    ASSERT_TRUE(result.isOk());
    auto& loader = result.value();

    auto tags = loader.getStringArray("item.tags");
    ASSERT_TRUE(tags.isOk());
    EXPECT_EQ(tags.value().size(), 3u);
    EXPECT_EQ(tags.value()[0], "fire");
    EXPECT_EQ(tags.value()[2], "quest");
}

TEST_F(DataLoaderTest, StringArrayTypeMismatch) {
    auto result = DataLoader::parse(R"(
        [item]
        tags = [1, 2, 3]
    )");
    ASSERT_TRUE(result.isOk());
    auto tags = result.value().getStringArray("item.tags");
    EXPECT_TRUE(tags.isError());
    EXPECT_EQ(tags.code(), ErrorCode::InvalidState);
}

TEST_F(DataLoaderTest, FloatAcceptsInteger) {
    auto result = DataLoader::parse("[d]\nv = 10");
    ASSERT_TRUE(result.isOk());
    auto f = result.value().getFloat("d.v");
    ASSERT_TRUE(f.isOk());
    EXPECT_DOUBLE_EQ(f.value(), 10.0);
}

// -- DataLoader::load (file-based) --

TEST_F(DataLoaderTest, LoadFromFile) {
    auto path = writeTempFile("[test]\nvalue = 99");
    auto result = DataLoader::load(path);
    ASSERT_TRUE(result.isOk());
    EXPECT_EQ(result.value().getInt("test.value").value(), 99);
    EXPECT_NE(result.value().sourceName().find("test.toml"), std::string::npos);
}

TEST_F(DataLoaderTest, LoadMissingFile) {
    auto result = DataLoader::load("/nonexistent/path/missing.toml");
    ASSERT_TRUE(result.isError());
    EXPECT_EQ(result.code(), ErrorCode::NotFound);
}

// -- DataRegistry --

TEST_F(DataLoaderTest, RegistryCachesResult) {
    auto path = writeTempFile("[cache]\nhit = true");
    DataRegistry registry;

    auto first = registry.get(path);
    ASSERT_TRUE(first.isOk());
    auto* ptr1 = first.value();

    auto second = registry.get(path);
    ASSERT_TRUE(second.isOk());
    auto* ptr2 = second.value();

    EXPECT_EQ(ptr1, ptr2); // Same pointer = cache hit
    EXPECT_EQ(registry.size(), 1u);
}

TEST_F(DataLoaderTest, RegistryReload) {
    auto path = writeTempFile("[data]\nversion = 1");
    DataRegistry registry;

    auto first = registry.get(path);
    ASSERT_TRUE(first.isOk());
    EXPECT_EQ(first.value()->getInt("data.version").value(), 1);

    // Overwrite file with new content
    writeTempFile("[data]\nversion = 2");

    auto reloaded = registry.reload(path);
    ASSERT_TRUE(reloaded.isOk());
    EXPECT_EQ(reloaded.value()->getInt("data.version").value(), 2);
    EXPECT_EQ(registry.size(), 1u);
}

TEST_F(DataLoaderTest, RegistryRemove) {
    auto path = writeTempFile("[rm]\nx = 1");
    DataRegistry registry;

    registry.get(path);
    EXPECT_EQ(registry.size(), 1u);

    registry.remove(path);
    EXPECT_EQ(registry.size(), 0u);
}

TEST_F(DataLoaderTest, RegistryClear) {
    auto p1 = writeTempFile("[a]\nx = 1", "a.toml");
    auto p2 = writeTempFile("[b]\ny = 2", "b.toml");
    DataRegistry registry;

    registry.get(p1);
    registry.get(p2);
    EXPECT_EQ(registry.size(), 2u);

    registry.clear();
    EXPECT_EQ(registry.size(), 0u);
}

// -- Schema file smoke tests --

TEST_F(DataLoaderTest, ItemSchemaLoads) {
    auto result = DataLoader::load("data/schema/item.toml");
    ASSERT_TRUE(result.isOk()) << result.message();
    auto& loader = result.value();

    EXPECT_EQ(loader.getString("item.id").value(), "iron_sword");
    EXPECT_EQ(loader.getInt("item.stats.damage").value(), 12);
    EXPECT_DOUBLE_EQ(loader.getFloat("item.stats.weight").value(), 3.5);

    auto tags = loader.getStringArray("item.tags.values");
    ASSERT_TRUE(tags.isOk());
    EXPECT_EQ(tags.value().size(), 3u);
}

TEST_F(DataLoaderTest, NpcSchemaLoads) {
    auto result = DataLoader::load("data/schema/npc.toml");
    ASSERT_TRUE(result.isOk()) << result.message();
    auto& loader = result.value();

    EXPECT_EQ(loader.getString("npc.id").value(), "village_blacksmith");
    EXPECT_EQ(loader.getInt("npc.stats.health").value(), 150);
    EXPECT_DOUBLE_EQ(loader.getFloat("npc.stats.speed").value(), 2.0);
    EXPECT_DOUBLE_EQ(loader.getFloat("npc.position.x").value(), 10.5);
}

} // namespace fabric
