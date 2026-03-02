#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>

#include "fabric/core/ConfigManager.hh"
#include "fabric/core/DefaultConfig.hh"

namespace fabric {

class ConfigManagerTest : public ::testing::Test {
  protected:
    std::filesystem::path writeTempFile(const std::string& content, const std::string& name = "test.toml") {
        auto dir = std::filesystem::temp_directory_path() / "fabric_config_test";
        std::filesystem::create_directories(dir);
        auto path = dir / name;
        std::ofstream ofs(path);
        ofs << content;
        ofs.close();
        return path;
    }

    void TearDown() override {
        auto dir = std::filesystem::temp_directory_path() / "fabric_config_test";
        std::filesystem::remove_all(dir);
    }
};

// -- Compiled defaults (Layer 0) --

TEST_F(ConfigManagerTest, DefaultsAvailableWithoutTOML) {
    ConfigManager config;
    EXPECT_EQ(config.get<int>("window.width"), DefaultConfig::kWindowWidth);
    EXPECT_EQ(config.get<int>("window.height"), DefaultConfig::kWindowHeight);
    EXPECT_EQ(config.get<bool>("window.fullscreen"), DefaultConfig::kFullscreen);
    EXPECT_EQ(config.get<bool>("renderer.vsync"), DefaultConfig::kVsync);
    EXPECT_EQ(config.get<std::string>("logging.level"), std::string("info"));
}

TEST_F(ConfigManagerTest, AllWindowDefaultsPresent) {
    ConfigManager config;
    EXPECT_TRUE(config.has("window.title"));
    EXPECT_TRUE(config.has("window.width"));
    EXPECT_TRUE(config.has("window.height"));
    EXPECT_TRUE(config.has("window.min_width"));
    EXPECT_TRUE(config.has("window.min_height"));
    EXPECT_TRUE(config.has("window.display"));
    EXPECT_TRUE(config.has("window.fullscreen"));
    EXPECT_TRUE(config.has("window.borderless"));
    EXPECT_TRUE(config.has("window.resizable"));
    EXPECT_TRUE(config.has("window.hidpi"));
    EXPECT_TRUE(config.has("window.maximized"));
}

// -- Engine config (Layer 1) --

TEST_F(ConfigManagerTest, EngineConfigOverridesDefaults) {
    auto path = writeTempFile(R"(
        [window]
        width = 1920
        height = 1080
    )",
                              "fabric.toml");

    ConfigManager config;
    config.loadEngineConfig(path);

    EXPECT_EQ(config.get<int>("window.width"), 1920);
    EXPECT_EQ(config.get<int>("window.height"), 1080);
    // Unset values still come from defaults
    EXPECT_EQ(config.get<bool>("window.fullscreen"), DefaultConfig::kFullscreen);
}

// -- App config (Layer 2) --

TEST_F(ConfigManagerTest, AppConfigOverridesEngine) {
    auto engine = writeTempFile(R"(
        [window]
        width = 1920
        title = "Engine"
    )",
                                "fabric.toml");

    auto app = writeTempFile(R"(
        [window]
        title = "Recurse"
        [gameplay]
        fov = 60.0
    )",
                             "recurse.toml");

    ConfigManager config;
    config.loadEngineConfig(engine);
    config.loadAppConfig(app);

    EXPECT_EQ(config.get<std::string>("window.title"), "Recurse");
    EXPECT_EQ(config.get<int>("window.width"), 1920); // from engine
    EXPECT_DOUBLE_EQ(*config.get<double>("gameplay.fov"), 60.0);
}

// -- User config (Layer 3) --

TEST_F(ConfigManagerTest, UserConfigOverridesApp) {
    auto app = writeTempFile(R"(
        [window]
        width = 1280
        height = 720
    )",
                             "recurse.toml");

    auto user = writeTempFile(R"(
        [window]
        width = 2560
        fullscreen = true
    )",
                              "user.toml");

    ConfigManager config;
    config.loadAppConfig(app);
    config.loadUserConfig(user);

    EXPECT_EQ(config.get<int>("window.width"), 2560);
    EXPECT_EQ(config.get<int>("window.height"), 720);       // from app
    EXPECT_EQ(config.get<bool>("window.fullscreen"), true); // from user
}

// -- CLI overrides (Layer 4) --

TEST_F(ConfigManagerTest, CLIOverridesAllLayers) {
    auto engine = writeTempFile(R"(
        [window]
        width = 1920
    )",
                                "fabric.toml");

    ConfigManager config;
    config.loadEngineConfig(engine);

    char arg0[] = "test";
    char arg1[] = "--window.width=3840";
    char arg2[] = "--renderer.debug=true";
    char arg3[] = "--gameplay.fov=90.0";
    char* args[] = {arg0, arg1, arg2, arg3};
    config.applyCLIOverrides(4, args);

    EXPECT_EQ(config.get<int>("window.width"), 3840);
    EXPECT_EQ(config.get<bool>("renderer.debug"), true);
    EXPECT_DOUBLE_EQ(*config.get<double>("gameplay.fov"), 90.0);
}

TEST_F(ConfigManagerTest, CLIStringValues) {
    ConfigManager config;
    char arg0[] = "test";
    char arg1[] = "--logging.level=trace";
    char* args[] = {arg0, arg1};
    config.applyCLIOverrides(2, args);

    EXPECT_EQ(config.get<std::string>("logging.level"), "trace");
}

// -- Full precedence chain --

TEST_F(ConfigManagerTest, FullPrecedenceChain) {
    auto engine = writeTempFile("[window]\nwidth = 800", "fabric.toml");
    auto app = writeTempFile("[window]\nwidth = 1024", "recurse.toml");
    auto user = writeTempFile("[window]\nwidth = 1920", "user.toml");

    ConfigManager config;
    config.loadEngineConfig(engine);
    config.loadAppConfig(app);
    config.loadUserConfig(user);

    char arg0[] = "test";
    char arg1[] = "--window.width=3840";
    char* args[] = {arg0, arg1};
    config.applyCLIOverrides(2, args);

    // CLI wins over all
    EXPECT_EQ(config.get<int>("window.width"), 3840);
}

// -- Typed access --

TEST_F(ConfigManagerTest, TypedAccessVariants) {
    auto path = writeTempFile(R"(
        [test]
        int_val = 42
        float_val = 3.14
        bool_val = true
        str_val = "hello"
    )",
                              "test.toml");

    ConfigManager config;
    config.loadEngineConfig(path);

    EXPECT_EQ(config.get<int>("test.int_val"), 42);
    EXPECT_EQ(config.get<int64_t>("test.int_val"), 42);
    EXPECT_NEAR(*config.get<double>("test.float_val"), 3.14, 0.001);
    EXPECT_NEAR(*config.get<float>("test.float_val"), 3.14f, 0.001f);
    EXPECT_EQ(config.get<bool>("test.bool_val"), true);
    EXPECT_EQ(config.get<std::string>("test.str_val"), "hello");
}

// -- Missing key --

TEST_F(ConfigManagerTest, MissingKeyReturnsNullopt) {
    ConfigManager config;
    EXPECT_FALSE(config.get<int>("nonexistent.key").has_value());
    EXPECT_FALSE(config.has("nonexistent.key"));
}

TEST_F(ConfigManagerTest, GetWithDefault) {
    ConfigManager config;
    EXPECT_EQ(config.get<int>("nonexistent.key", 999), 999);
    EXPECT_EQ(config.get<std::string>("nonexistent.key", std::string("fallback")), "fallback");
    // Existing key ignores default
    EXPECT_EQ(config.get<int>("window.width", 999), DefaultConfig::kWindowWidth);
}

// -- set() and dirty tracking --

TEST_F(ConfigManagerTest, SetWritesToUserLayer) {
    ConfigManager config;
    config.set<int>("window.width", 2560);

    EXPECT_EQ(config.get<int>("window.width"), 2560);
}

TEST_F(ConfigManagerTest, SetStringValue) {
    ConfigManager config;
    config.set<std::string>("window.title", std::string("Custom"));

    EXPECT_EQ(config.get<std::string>("window.title"), "Custom");
}

// -- Debounced flush --

TEST_F(ConfigManagerTest, FlushIfDirtyRespectsDebounce) {
    auto userPath = std::filesystem::temp_directory_path() / "fabric_config_test" / "debounce_user.toml";

    ConfigManager config;
    config.setUserConfigPath(userPath);
    config.set<int>("test.value", 42);

    // Immediately after set, debounce not yet elapsed
    config.flushIfDirty();
    EXPECT_FALSE(std::filesystem::exists(userPath));
}

TEST_F(ConfigManagerTest, FlushNowWritesImmediately) {
    auto userPath = std::filesystem::temp_directory_path() / "fabric_config_test" / "flush_user.toml";
    std::filesystem::create_directories(userPath.parent_path());

    ConfigManager config;
    config.setUserConfigPath(userPath);
    config.set<int>("audio.volume", 80);
    config.flushNow();

    ASSERT_TRUE(std::filesystem::exists(userPath));

    // Verify file content is valid TOML with the value we set
    auto tbl = toml::parse_file(userPath.string());
    EXPECT_EQ(tbl.at_path("audio.volume").value<int64_t>(), 80);
}

// -- Missing TOML file handling --

TEST_F(ConfigManagerTest, MissingTOMLFileSkipsGracefully) {
    ConfigManager config;
    // Loading a non-existent file should not throw, just skip
    config.loadEngineConfig("/nonexistent/fabric.toml");
    config.loadAppConfig("/nonexistent/recurse.toml");
    config.loadUserConfig("/nonexistent/user.toml");

    // Defaults still work
    EXPECT_EQ(config.get<int>("window.width"), DefaultConfig::kWindowWidth);
}

// -- Section enumeration / nested access --

TEST_F(ConfigManagerTest, NestedSectionAccess) {
    auto path = writeTempFile(R"(
        [gameplay.character]
        walk_speed = 5.0
        flight_speed = 12.0
    )",
                              "recurse.toml");

    ConfigManager config;
    config.loadAppConfig(path);

    EXPECT_DOUBLE_EQ(*config.get<double>("gameplay.character.walk_speed"), 5.0);
    EXPECT_DOUBLE_EQ(*config.get<double>("gameplay.character.flight_speed"), 12.0);
    EXPECT_TRUE(config.has("gameplay.character"));
    EXPECT_TRUE(config.has("gameplay"));
}

// -- Malformed TOML handling --

TEST_F(ConfigManagerTest, MalformedTOMLFileSkipsGracefully) {
    auto path = writeTempFile("this is not valid [[ toml ===", "bad.toml");

    ConfigManager config;
    config.loadEngineConfig(path);

    // Defaults still intact after failed parse
    EXPECT_EQ(config.get<int>("window.width"), DefaultConfig::kWindowWidth);
}

TEST_F(ConfigManagerTest, MalformedAppConfigPreservesExistingLayers) {
    auto engine = writeTempFile("[window]\nwidth = 1920", "fabric.toml");
    auto badApp = writeTempFile("not valid toml {{{{", "bad_app.toml");

    ConfigManager config;
    config.loadEngineConfig(engine);
    config.loadAppConfig(badApp);

    // Engine layer value survives bad app config
    EXPECT_EQ(config.get<int>("window.width"), 1920);
}

TEST_F(ConfigManagerTest, MalformedUserConfigPreservesExistingLayers) {
    auto app = writeTempFile("[window]\ntitle = \"MyApp\"", "app.toml");
    auto badUser = writeTempFile("[invalid\nwhat", "bad_user.toml");

    ConfigManager config;
    config.loadAppConfig(app);
    config.loadUserConfig(badUser);

    EXPECT_EQ(config.get<std::string>("window.title"), "MyApp");
}

// -- Empty TOML files --

TEST_F(ConfigManagerTest, EmptyTOMLFileLoadsWithoutError) {
    auto path = writeTempFile("", "empty.toml");

    ConfigManager config;
    config.loadEngineConfig(path);

    // Defaults intact
    EXPECT_EQ(config.get<int>("window.width"), DefaultConfig::kWindowWidth);
}

TEST_F(ConfigManagerTest, EmptyAppConfigPreservesEngine) {
    auto engine = writeTempFile("[window]\nwidth = 1920", "fabric.toml");
    auto emptyApp = writeTempFile("", "empty_app.toml");

    ConfigManager config;
    config.loadEngineConfig(engine);
    config.loadAppConfig(emptyApp);

    EXPECT_EQ(config.get<int>("window.width"), 1920);
}

// -- Type mismatch in get<T> --

TEST_F(ConfigManagerTest, TypeMismatchReturnsNullopt) {
    auto path = writeTempFile(R"(
        [test]
        str_val = "hello"
        int_val = 42
        bool_val = true
    )",
                              "types.toml");

    ConfigManager config;
    config.loadEngineConfig(path);

    // Reading string as int should return nullopt (no coercion)
    EXPECT_FALSE(config.get<int>("test.str_val").has_value());
    // Reading string as bool should return nullopt (no coercion)
    EXPECT_FALSE(config.get<bool>("test.str_val").has_value());
}

TEST_F(ConfigManagerTest, TypeMismatchWithDefaultReturnsDefault) {
    auto path = writeTempFile("[test]\nstr_val = \"hello\"", "types.toml");

    ConfigManager config;
    config.loadEngineConfig(path);

    // String to int mismatch falls back to provided default
    EXPECT_EQ(config.get<int>("test.str_val", 999), 999);
    // String to bool mismatch falls back to provided default
    EXPECT_EQ(config.get<bool>("test.str_val", true), true);
}

// -- CLI flag edge cases --

TEST_F(ConfigManagerTest, CLIIgnoresNonFlagArguments) {
    ConfigManager config;
    char arg0[] = "test";
    char arg1[] = "positional_arg";
    char arg2[] = "-single-dash";
    char arg3[] = "--window.width=1920";
    char* args[] = {arg0, arg1, arg2, arg3};
    config.applyCLIOverrides(4, args);

    EXPECT_EQ(config.get<int>("window.width"), 1920);
    // Non-flag args should not pollute config
    EXPECT_FALSE(config.has("positional_arg"));
}

TEST_F(ConfigManagerTest, CLIIgnoresFlagsWithoutEquals) {
    ConfigManager config;
    char arg0[] = "test";
    char arg1[] = "--verbose";
    char arg2[] = "--window.width=1024";
    char* args[] = {arg0, arg1, arg2};
    config.applyCLIOverrides(3, args);

    EXPECT_EQ(config.get<int>("window.width"), 1024);
    EXPECT_FALSE(config.has("verbose"));
}

TEST_F(ConfigManagerTest, CLIDuplicateFlagLastWins) {
    ConfigManager config;
    char arg0[] = "test";
    char arg1[] = "--window.width=800";
    char arg2[] = "--window.width=1600";
    char* args[] = {arg0, arg1, arg2};
    config.applyCLIOverrides(3, args);

    EXPECT_EQ(config.get<int>("window.width"), 1600);
}

TEST_F(ConfigManagerTest, CLIBooleanParsing) {
    ConfigManager config;
    char arg0[] = "test";
    char arg1[] = "--renderer.debug=true";
    char arg2[] = "--window.fullscreen=false";
    char* args[] = {arg0, arg1, arg2};
    config.applyCLIOverrides(3, args);

    EXPECT_EQ(config.get<bool>("renderer.debug"), true);
    EXPECT_EQ(config.get<bool>("window.fullscreen"), false);
}

TEST_F(ConfigManagerTest, CLIEmptyArgvIsNoOp) {
    ConfigManager config;
    char arg0[] = "test";
    char* args[] = {arg0};
    config.applyCLIOverrides(1, args);

    // Defaults still intact
    EXPECT_EQ(config.get<int>("window.width"), DefaultConfig::kWindowWidth);
}

// -- set() with deep nested paths --

TEST_F(ConfigManagerTest, SetCreatesIntermediateTables) {
    ConfigManager config;
    config.set<int>("new_section.subsection.deep.value", 42);

    EXPECT_EQ(config.get<int>("new_section.subsection.deep.value"), 42);
    EXPECT_TRUE(config.has("new_section.subsection.deep"));
    EXPECT_TRUE(config.has("new_section.subsection"));
    EXPECT_TRUE(config.has("new_section"));
}

TEST_F(ConfigManagerTest, SetOverwritesExistingValue) {
    ConfigManager config;
    config.set<int>("window.width", 2560);
    config.set<int>("window.width", 3840);

    EXPECT_EQ(config.get<int>("window.width"), 3840);
}

TEST_F(ConfigManagerTest, SetFloatValue) {
    ConfigManager config;
    config.set<float>("gameplay.fov", 90.0f);

    EXPECT_NEAR(*config.get<float>("gameplay.fov"), 90.0f, 0.01f);
}

TEST_F(ConfigManagerTest, SetBoolValue) {
    ConfigManager config;
    config.set<bool>("window.fullscreen", true);

    EXPECT_EQ(config.get<bool>("window.fullscreen"), true);
}

// -- Flush behavior --

TEST_F(ConfigManagerTest, FlushNowWhenNotDirtyIsNoOp) {
    auto userPath = std::filesystem::temp_directory_path() / "fabric_config_test" / "noop_user.toml";
    std::filesystem::create_directories(userPath.parent_path());

    ConfigManager config;
    config.setUserConfigPath(userPath);

    // No set() called, so not dirty
    config.flushNow();
    EXPECT_FALSE(std::filesystem::exists(userPath));
}

TEST_F(ConfigManagerTest, FlushIfDirtyWritesAfterDebounce) {
    auto userPath = std::filesystem::temp_directory_path() / "fabric_config_test" / "delayed_user.toml";
    std::filesystem::create_directories(userPath.parent_path());

    ConfigManager config;
    config.setUserConfigPath(userPath);
    config.set<int>("test.val", 123);

    // Wait for debounce (500ms + margin)
    std::this_thread::sleep_for(std::chrono::milliseconds(550));

    config.flushIfDirty();
    ASSERT_TRUE(std::filesystem::exists(userPath));

    auto tbl = toml::parse_file(userPath.string());
    EXPECT_EQ(tbl.at_path("test.val").value<int64_t>(), 123);
}

// -- DefaultConfig constexpr verification --

TEST_F(ConfigManagerTest, DefaultConfigConstexprValues) {
    // Verify the compiled defaults have expected types and values
    static_assert(DefaultConfig::kWindowWidth == 1280);
    static_assert(DefaultConfig::kWindowHeight == 720);
    static_assert(DefaultConfig::kMinWindowWidth == 640);
    static_assert(DefaultConfig::kMinWindowHeight == 480);
    static_assert(DefaultConfig::kDisplay == 0);
    static_assert(DefaultConfig::kFullscreen == false);
    static_assert(DefaultConfig::kBorderless == false);
    static_assert(DefaultConfig::kResizable == true);
    static_assert(DefaultConfig::kHiDPI == true);
    static_assert(DefaultConfig::kMaximized == false);
    static_assert(DefaultConfig::kRendererDebug == false);
    static_assert(DefaultConfig::kVsync == true);
    static_assert(DefaultConfig::kProfilingEnabled == false);
    static_assert(DefaultConfig::kMimallocOverride == false);
    static_assert(DefaultConfig::kFileSink == true);
    static_assert(DefaultConfig::kConsoleSink == true);

    // Runtime check that string constants are accessible
    EXPECT_STREQ(DefaultConfig::kWindowTitle, "Fabric");
    EXPECT_STREQ(DefaultConfig::kRendererBackend, "vulkan");
    EXPECT_STREQ(DefaultConfig::kLogLevel, "info");
    EXPECT_STREQ(DefaultConfig::kProfilingConnectAddress, "");
}

// -- Deep merge with array values --

TEST_F(ConfigManagerTest, DeepMergeArrayReplacement) {
    auto engine = writeTempFile(R"(
        [assets]
        search_paths = ["data/", "res/"]
    )",
                                "fabric.toml");

    auto app = writeTempFile(R"(
        [assets]
        search_paths = ["app_data/"]
    )",
                             "recurse.toml");

    ConfigManager config;
    config.loadEngineConfig(engine);
    config.loadAppConfig(app);

    // Arrays are replaced wholesale, not merged
    const auto& merged = config.merged();
    auto* arr = merged.at_path("assets.search_paths").as_array();
    ASSERT_NE(arr, nullptr);
    EXPECT_EQ(arr->size(), 1u);
    EXPECT_EQ(arr->at(0).value<std::string>(), "app_data/");
}

// -- Layer isolation --

TEST_F(ConfigManagerTest, SetDoesNotAffectLowerLayers) {
    auto engine = writeTempFile("[window]\nwidth = 1920", "fabric.toml");

    ConfigManager config;
    config.loadEngineConfig(engine);

    // set() writes to user layer only
    config.set<int>("window.width", 3840);
    EXPECT_EQ(config.get<int>("window.width"), 3840);

    // Reloading engine config should not lose user override
    // (user layer is above engine in precedence)
    config.loadEngineConfig(engine);
    EXPECT_EQ(config.get<int>("window.width"), 3840);
}

} // namespace fabric
