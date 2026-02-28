#include "fabric/core/DataLoader.hh"
#include "fabric/utils/ErrorHandling.hh"
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <string>
#include <vector>

using namespace fabric;

// -- Test structs ------------------------------------------------------------

struct ItemDef {
    std::string id;
    std::string name;
    std::string category;
    bool stackable = false;
    int64_t maxStack = 1;
    double weight = 0.0;
};

static Result<ItemDef> deserializeItem(const toml::table& t) {
    auto id = getString(t, "id");
    if (id.isError())
        return Result<ItemDef>::error(id.code(), id.message());

    auto name = getString(t, "name");
    if (name.isError())
        return Result<ItemDef>::error(name.code(), name.message());

    auto category = getString(t, "category");
    if (category.isError())
        return Result<ItemDef>::error(category.code(), category.message());

    ItemDef item;
    item.id = std::move(id.value());
    item.name = std::move(name.value());
    item.category = std::move(category.value());
    item.stackable = getBoolOr(t, "stackable", false);
    item.maxStack = getIntOr(t, "max_stack", 1);
    item.weight = getFloatOr(t, "weight", 0.0);
    return Result<ItemDef>::ok(std::move(item));
}

// -- Helper: write temp TOML file --------------------------------------------

class DataLoaderTest : public ::testing::Test {
  protected:
    void SetUp() override {
        tempDir_ = std::filesystem::temp_directory_path() / "fabric_dataloader_test";
        std::filesystem::create_directories(tempDir_);
    }

    void TearDown() override { std::filesystem::remove_all(tempDir_); }

    std::filesystem::path writeTempFile(const std::string& name, const std::string& content) {
        auto path = tempDir_ / name;
        std::ofstream out(path);
        out << content;
        out.close();
        return path;
    }

    std::filesystem::path tempDir_;
};

// -- parseTomlString tests ---------------------------------------------------

TEST(DataLoaderParseTest, ParseValidString) {
    auto result = parseTomlString(R"(
        name = "test"
        value = 42
    )");
    ASSERT_TRUE(result.isOk());
    EXPECT_EQ(result.value()["name"].value<std::string>(), "test");
    EXPECT_EQ(result.value()["value"].value<int64_t>(), 42);
}

TEST(DataLoaderParseTest, ParseMalformedStringReturnsError) {
    auto result = parseTomlString(R"(
        name = "unterminated
    )");
    ASSERT_TRUE(result.isError());
    EXPECT_EQ(result.code(), ErrorCode::InvalidState);
    EXPECT_FALSE(result.message().empty());
}

TEST(DataLoaderParseTest, ParseEmptyStringReturnsEmptyTable) {
    auto result = parseTomlString("");
    ASSERT_TRUE(result.isOk());
    EXPECT_TRUE(result.value().empty());
}

// -- Typed extraction helper tests -------------------------------------------

TEST(DataLoaderExtractTest, GetStringValid) {
    auto table = parseTomlString(R"(name = "hello")");
    ASSERT_TRUE(table.isOk());
    auto val = getString(table.value(), "name");
    ASSERT_TRUE(val.isOk());
    EXPECT_EQ(val.value(), "hello");
}

TEST(DataLoaderExtractTest, GetStringMissing) {
    auto table = parseTomlString(R"(other = 1)");
    ASSERT_TRUE(table.isOk());
    auto val = getString(table.value(), "name");
    ASSERT_TRUE(val.isError());
    EXPECT_EQ(val.code(), ErrorCode::NotFound);
}

TEST(DataLoaderExtractTest, GetStringTypeMismatch) {
    auto table = parseTomlString(R"(name = 42)");
    ASSERT_TRUE(table.isOk());
    auto val = getString(table.value(), "name");
    ASSERT_TRUE(val.isError());
    EXPECT_EQ(val.code(), ErrorCode::InvalidState);
}

TEST(DataLoaderExtractTest, GetIntValid) {
    auto table = parseTomlString(R"(count = 100)");
    ASSERT_TRUE(table.isOk());
    auto val = getInt(table.value(), "count");
    ASSERT_TRUE(val.isOk());
    EXPECT_EQ(val.value(), 100);
}

TEST(DataLoaderExtractTest, GetIntMissing) {
    auto table = parseTomlString(R"(other = "x")");
    ASSERT_TRUE(table.isOk());
    auto val = getInt(table.value(), "count");
    ASSERT_TRUE(val.isError());
    EXPECT_EQ(val.code(), ErrorCode::NotFound);
}

TEST(DataLoaderExtractTest, GetIntTypeMismatch) {
    auto table = parseTomlString(R"(count = "not_a_number")");
    ASSERT_TRUE(table.isOk());
    auto val = getInt(table.value(), "count");
    ASSERT_TRUE(val.isError());
    EXPECT_EQ(val.code(), ErrorCode::InvalidState);
}

TEST(DataLoaderExtractTest, GetFloatValid) {
    auto table = parseTomlString(R"(weight = 3.14)");
    ASSERT_TRUE(table.isOk());
    auto val = getFloat(table.value(), "weight");
    ASSERT_TRUE(val.isOk());
    EXPECT_NEAR(val.value(), 3.14, 0.001);
}

TEST(DataLoaderExtractTest, GetFloatFromInt) {
    auto table = parseTomlString(R"(weight = 5)");
    ASSERT_TRUE(table.isOk());
    auto val = getFloat(table.value(), "weight");
    ASSERT_TRUE(val.isOk());
    EXPECT_NEAR(val.value(), 5.0, 0.001);
}

TEST(DataLoaderExtractTest, GetFloatTypeMismatch) {
    auto table = parseTomlString(R"(weight = "heavy")");
    ASSERT_TRUE(table.isOk());
    auto val = getFloat(table.value(), "weight");
    ASSERT_TRUE(val.isError());
    EXPECT_EQ(val.code(), ErrorCode::InvalidState);
}

TEST(DataLoaderExtractTest, GetBoolValid) {
    auto table = parseTomlString(R"(enabled = true)");
    ASSERT_TRUE(table.isOk());
    auto val = getBool(table.value(), "enabled");
    ASSERT_TRUE(val.isOk());
    EXPECT_TRUE(val.value());
}

TEST(DataLoaderExtractTest, GetBoolTypeMismatch) {
    auto table = parseTomlString(R"(enabled = "yes")");
    ASSERT_TRUE(table.isOk());
    auto val = getBool(table.value(), "enabled");
    ASSERT_TRUE(val.isError());
    EXPECT_EQ(val.code(), ErrorCode::InvalidState);
}

TEST(DataLoaderExtractTest, GetTableValid) {
    auto table = parseTomlString(R"(
        [stats]
        health = 100
    )");
    ASSERT_TRUE(table.isOk());
    auto val = getTable(table.value(), "stats");
    ASSERT_TRUE(val.isOk());
    EXPECT_EQ((*val.value())["health"].value<int64_t>(), 100);
}

TEST(DataLoaderExtractTest, GetTableMissing) {
    auto table = parseTomlString(R"(name = "test")");
    ASSERT_TRUE(table.isOk());
    auto val = getTable(table.value(), "stats");
    ASSERT_TRUE(val.isError());
    EXPECT_EQ(val.code(), ErrorCode::NotFound);
}

TEST(DataLoaderExtractTest, GetArrayValid) {
    auto table = parseTomlString(R"(tags = ["a", "b", "c"])");
    ASSERT_TRUE(table.isOk());
    auto val = getArray(table.value(), "tags");
    ASSERT_TRUE(val.isOk());
    EXPECT_EQ(val.value()->size(), 3u);
}

// -- Optional variant tests --------------------------------------------------

TEST(DataLoaderExtractTest, GetStringOrDefault) {
    auto table = parseTomlString(R"(other = 1)");
    ASSERT_TRUE(table.isOk());
    EXPECT_EQ(getStringOr(table.value(), "missing", "fallback"), "fallback");
}

TEST(DataLoaderExtractTest, GetStringOrPresent) {
    auto table = parseTomlString(R"(name = "hello")");
    ASSERT_TRUE(table.isOk());
    EXPECT_EQ(getStringOr(table.value(), "name", "fallback"), "hello");
}

TEST(DataLoaderExtractTest, GetIntOrDefault) {
    auto table = parseTomlString(R"(other = "x")");
    ASSERT_TRUE(table.isOk());
    EXPECT_EQ(getIntOr(table.value(), "missing", 99), 99);
}

TEST(DataLoaderExtractTest, GetFloatOrFromInt) {
    auto table = parseTomlString(R"(val = 7)");
    ASSERT_TRUE(table.isOk());
    EXPECT_NEAR(getFloatOr(table.value(), "val", 0.0), 7.0, 0.001);
}

TEST(DataLoaderExtractTest, GetBoolOrDefault) {
    auto table = parseTomlString(R"(other = 1)");
    ASSERT_TRUE(table.isOk());
    EXPECT_TRUE(getBoolOr(table.value(), "missing", true));
}

// -- parseTomlFile tests (disk I/O) ------------------------------------------

TEST_F(DataLoaderTest, ParseValidFile) {
    auto path = writeTempFile("valid.toml", R"(
        title = "Test Config"
        value = 42
    )");
    auto result = parseTomlFile(path);
    ASSERT_TRUE(result.isOk());
    EXPECT_EQ(result.value()["title"].value<std::string>(), "Test Config");
}

TEST_F(DataLoaderTest, ParseNonexistentFile) {
    auto result = parseTomlFile(tempDir_ / "does_not_exist.toml");
    ASSERT_TRUE(result.isError());
    EXPECT_EQ(result.code(), ErrorCode::NotFound);
}

TEST_F(DataLoaderTest, ParseMalformedFile) {
    auto path = writeTempFile("bad.toml", R"(
        key = "unterminated
    )");
    auto result = parseTomlFile(path);
    ASSERT_TRUE(result.isError());
    EXPECT_EQ(result.code(), ErrorCode::InvalidState);
    // Error message should reference the file path
    EXPECT_NE(result.message().find("bad.toml"), std::string::npos);
}

// -- DataLoader::load() tests ------------------------------------------------

TEST_F(DataLoaderTest, LoadTypedStruct) {
    auto path = writeTempFile("item.toml", R"(
        id = "sword_iron"
        name = "Iron Sword"
        category = "weapon"
        stackable = false
        max_stack = 1
        weight = 3.5
    )");

    auto result = DataLoader::load<ItemDef>(path, deserializeItem);
    ASSERT_TRUE(result.isOk());
    EXPECT_EQ(result.value().id, "sword_iron");
    EXPECT_EQ(result.value().name, "Iron Sword");
    EXPECT_EQ(result.value().category, "weapon");
    EXPECT_FALSE(result.value().stackable);
    EXPECT_EQ(result.value().maxStack, 1);
    EXPECT_NEAR(result.value().weight, 3.5, 0.001);
}

TEST_F(DataLoaderTest, LoadMissingRequiredKey) {
    auto path = writeTempFile("item_bad.toml", R"(
        id = "test"
    )");

    auto result = DataLoader::load<ItemDef>(path, deserializeItem);
    ASSERT_TRUE(result.isError());
    EXPECT_EQ(result.code(), ErrorCode::NotFound);
}

TEST_F(DataLoaderTest, LoadTypeMismatchKey) {
    auto path = writeTempFile("item_type.toml", R"(
        id = 42
        name = "Test"
        category = "test"
    )");

    auto result = DataLoader::load<ItemDef>(path, deserializeItem);
    ASSERT_TRUE(result.isError());
    EXPECT_EQ(result.code(), ErrorCode::InvalidState);
}

// -- DataLoader::loadAll() tests ---------------------------------------------

TEST_F(DataLoaderTest, LoadAllArrayOfTables) {
    auto path = writeTempFile("items.toml", R"(
        [[item]]
        id = "sword"
        name = "Sword"
        category = "weapon"

        [[item]]
        id = "potion"
        name = "Potion"
        category = "consumable"
        stackable = true
        max_stack = 64
    )");

    auto result = DataLoader::loadAll<ItemDef>(path, "item", deserializeItem);
    ASSERT_TRUE(result.isOk());
    ASSERT_EQ(result.value().size(), 2u);
    EXPECT_EQ(result.value()[0].id, "sword");
    EXPECT_EQ(result.value()[1].id, "potion");
    EXPECT_TRUE(result.value()[1].stackable);
    EXPECT_EQ(result.value()[1].maxStack, 64);
}

TEST_F(DataLoaderTest, LoadAllMissingArrayKey) {
    auto path = writeTempFile("empty.toml", R"(
        title = "No items here"
    )");

    auto result = DataLoader::loadAll<ItemDef>(path, "item", deserializeItem);
    ASSERT_TRUE(result.isError());
    EXPECT_EQ(result.code(), ErrorCode::NotFound);
}

TEST_F(DataLoaderTest, LoadAllBadElement) {
    auto path = writeTempFile("items_bad.toml", R"(
        [[item]]
        id = "good"
        name = "Good Item"
        category = "weapon"

        [[item]]
        id = 42
        name = "Bad Item"
        category = "broken"
    )");

    auto result = DataLoader::loadAll<ItemDef>(path, "item", deserializeItem);
    ASSERT_TRUE(result.isError());
    // Error should mention element index
    EXPECT_NE(result.message().find("element 1"), std::string::npos);
}

// -- DataRegistry tests ------------------------------------------------------

TEST_F(DataLoaderTest, RegistryGetCaches) {
    auto path = writeTempFile("config.toml", R"(
        [window]
        width = 1920
    )");

    DataRegistry registry;
    auto result1 = registry.get(path);
    ASSERT_TRUE(result1.isOk());
    EXPECT_EQ(registry.size(), 1u);

    // Second get should return cached pointer
    auto result2 = registry.get(path);
    ASSERT_TRUE(result2.isOk());
    EXPECT_EQ(registry.size(), 1u);
    EXPECT_EQ(result1.value(), result2.value());
}

TEST_F(DataLoaderTest, RegistryReloadInvalidatesCache) {
    auto path = writeTempFile("config.toml", R"(
        [window]
        width = 1920
    )");

    DataRegistry registry;
    auto result1 = registry.get(path);
    ASSERT_TRUE(result1.isOk());

    auto* original = result1.value();
    auto width1 = getInt(*original, "window");

    // Modify file on disk
    writeTempFile("config.toml", R"(
        [window]
        width = 2560
    )");

    auto result2 = registry.reload(path);
    ASSERT_TRUE(result2.isOk());
    EXPECT_EQ(registry.size(), 1u);

    auto windowResult = getTable(*result2.value(), "window");
    ASSERT_TRUE(windowResult.isOk());
    auto newWidth = getInt(*windowResult.value(), "width");
    ASSERT_TRUE(newWidth.isOk());
    EXPECT_EQ(newWidth.value(), 2560);
}

TEST_F(DataLoaderTest, RegistryClear) {
    auto path = writeTempFile("config.toml", R"(title = "test")");

    DataRegistry registry;
    registry.get(path);
    EXPECT_EQ(registry.size(), 1u);

    registry.clear();
    EXPECT_EQ(registry.size(), 0u);
}

TEST_F(DataLoaderTest, RegistryContains) {
    auto path = writeTempFile("config.toml", R"(title = "test")");

    DataRegistry registry;
    EXPECT_FALSE(registry.contains(path));

    registry.get(path);
    EXPECT_TRUE(registry.contains(path));
}

TEST_F(DataLoaderTest, RegistryGetNonexistentFile) {
    DataRegistry registry;
    auto result = registry.get(tempDir_ / "missing.toml");
    ASSERT_TRUE(result.isError());
    EXPECT_EQ(result.code(), ErrorCode::NotFound);
}

// -- Schema file parsing tests (validates the example schemas) ---------------

TEST_F(DataLoaderTest, ParseExampleSchemaFiles) {
    // These paths are relative to the project root, so we need to find them.
    // In CI, the working directory is the project root.
    auto projectRoot = std::filesystem::current_path();
    auto itemPath = projectRoot / "data" / "schema" / "item.toml";
    auto npcPath = projectRoot / "data" / "schema" / "npc.toml";
    auto configPath = projectRoot / "data" / "schema" / "config.toml";

    if (std::filesystem::exists(itemPath)) {
        auto result = parseTomlFile(itemPath);
        ASSERT_TRUE(result.isOk()) << "Failed to parse item.toml: " << result.message();

        auto items = result.value().get_as<toml::array>("item");
        ASSERT_NE(items, nullptr);
        EXPECT_GE(items->size(), 1u);
    }

    if (std::filesystem::exists(npcPath)) {
        auto result = parseTomlFile(npcPath);
        ASSERT_TRUE(result.isOk()) << "Failed to parse npc.toml: " << result.message();

        auto npcs = result.value().get_as<toml::array>("npc");
        ASSERT_NE(npcs, nullptr);
        EXPECT_GE(npcs->size(), 1u);
    }

    if (std::filesystem::exists(configPath)) {
        auto result = parseTomlFile(configPath);
        ASSERT_TRUE(result.isOk()) << "Failed to parse config.toml: " << result.message();

        auto windowTable = getTable(result.value(), "window");
        ASSERT_TRUE(windowTable.isOk());
        auto width = getInt(*windowTable.value(), "width");
        ASSERT_TRUE(width.isOk());
        EXPECT_EQ(width.value(), 1920);
    }
}
