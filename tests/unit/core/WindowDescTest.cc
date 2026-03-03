#include "fabric/platform/WindowDesc.hh"
#include "fixtures/SDLFixture.hh"

#include <gtest/gtest.h>

#include <SDL3/SDL.h>
#include <toml++/toml.hpp>

using namespace fabric;
using fabric::test::SDLFixture;

TEST(WindowDescTest, DefaultValues) {
    WindowDesc desc;
    EXPECT_EQ(desc.title, "Fabric");
    EXPECT_EQ(desc.width, 1280);
    EXPECT_EQ(desc.height, 720);
    EXPECT_EQ(desc.minWidth, 640);
    EXPECT_EQ(desc.minHeight, 480);
    EXPECT_EQ(desc.posX, -1);
    EXPECT_EQ(desc.posY, -1);
    EXPECT_EQ(desc.displayIndex, 0u);
    EXPECT_TRUE(desc.vulkan);
    EXPECT_TRUE(desc.hidpiEnabled);
    EXPECT_TRUE(desc.resizable);
    EXPECT_FALSE(desc.borderless);
    EXPECT_FALSE(desc.fullscreen);
    EXPECT_FALSE(desc.fullscreenDesktop);
    EXPECT_FALSE(desc.maximized);
    EXPECT_FALSE(desc.hidden);
}

TEST(WindowDescTest, DefaultFlagsMapping) {
    WindowDesc desc;
    uint64_t flags = desc.toSDLFlags();

    EXPECT_NE(flags & SDL_WINDOW_VULKAN, 0u);
    EXPECT_NE(flags & SDL_WINDOW_HIGH_PIXEL_DENSITY, 0u);
    EXPECT_NE(flags & SDL_WINDOW_RESIZABLE, 0u);
    EXPECT_EQ(flags & SDL_WINDOW_BORDERLESS, 0u);
    EXPECT_EQ(flags & SDL_WINDOW_FULLSCREEN, 0u);
    EXPECT_EQ(flags & SDL_WINDOW_MAXIMIZED, 0u);
    EXPECT_EQ(flags & SDL_WINDOW_HIDDEN, 0u);
}

TEST(WindowDescTest, FullscreenBorderlessFlags) {
    WindowDesc desc;
    desc.fullscreen = true;
    desc.borderless = true;
    desc.resizable = false;
    uint64_t flags = desc.toSDLFlags();

    EXPECT_NE(flags & SDL_WINDOW_FULLSCREEN, 0u);
    EXPECT_NE(flags & SDL_WINDOW_BORDERLESS, 0u);
    EXPECT_EQ(flags & SDL_WINDOW_RESIZABLE, 0u);
}

TEST(WindowDescTest, HiddenMaximizedFlags) {
    WindowDesc desc;
    desc.hidden = true;
    desc.maximized = true;
    uint64_t flags = desc.toSDLFlags();

    EXPECT_NE(flags & SDL_WINDOW_HIDDEN, 0u);
    EXPECT_NE(flags & SDL_WINDOW_MAXIMIZED, 0u);
}

TEST(WindowDescTest, NoVulkanFlag) {
    WindowDesc desc;
    desc.vulkan = false;
    uint64_t flags = desc.toSDLFlags();

    EXPECT_EQ(flags & SDL_WINDOW_VULKAN, 0u);
}

TEST(WindowDescTest, FromConfigOverridesDefaults) {
    auto table = toml::parse(R"(
        title = "TestApp"
        width = 1920
        height = 1080
        min_width = 800
        min_height = 600
        display = 1
        fullscreen = true
        borderless = false
        resizable = false
        hidpi = false
        maximized = true
    )");

    WindowDesc desc = windowDescFromConfig(table);

    EXPECT_EQ(desc.title, "TestApp");
    EXPECT_EQ(desc.width, 1920);
    EXPECT_EQ(desc.height, 1080);
    EXPECT_EQ(desc.minWidth, 800);
    EXPECT_EQ(desc.minHeight, 600);
    EXPECT_EQ(desc.displayIndex, 1u);
    EXPECT_TRUE(desc.fullscreen);
    EXPECT_FALSE(desc.borderless);
    EXPECT_FALSE(desc.resizable);
    EXPECT_FALSE(desc.hidpiEnabled);
    EXPECT_TRUE(desc.maximized);
}

TEST(WindowDescTest, FromConfigRetainsDefaultsForMissingKeys) {
    auto table = toml::parse(R"(
        title = "Partial"
    )");

    WindowDesc desc = windowDescFromConfig(table);

    EXPECT_EQ(desc.title, "Partial");
    EXPECT_EQ(desc.width, 1280);
    EXPECT_EQ(desc.height, 720);
    EXPECT_TRUE(desc.vulkan);
    EXPECT_TRUE(desc.resizable);
}

TEST(WindowDescTest, ApplyConstraintsWithNullWindow) {
    WindowDesc desc;
    desc.applyConstraints(nullptr); // should not crash
}

TEST_F(SDLFixture, CreateWindowReturnsValidHandle) {
    WindowDesc desc;
    desc.title = "TestWindow";
    desc.width = 320;
    desc.height = 240;
    desc.hidden = true;  // keep invisible during test
    desc.vulkan = false; // no GPU required for window creation test

    SDL_Window* window = createWindow(desc);
    ASSERT_NE(window, nullptr);

    EXPECT_EQ(std::string(SDL_GetWindowTitle(window)), "TestWindow");

    SDL_DestroyWindow(window);
}

// -- Zero and negative dimensions --

TEST(WindowDescTest, ZeroDimensions) {
    WindowDesc desc;
    desc.width = 0;
    desc.height = 0;

    // toSDLFlags should still work (flag mapping is independent of dimensions)
    uint64_t flags = desc.toSDLFlags();
    EXPECT_NE(flags & SDL_WINDOW_VULKAN, 0u);
}

TEST(WindowDescTest, NegativeDimensions) {
    WindowDesc desc;
    desc.width = -1;
    desc.height = -1;
    desc.minWidth = -100;
    desc.minHeight = -100;

    // Struct allows negative values (SDL will reject them at creation time)
    EXPECT_EQ(desc.width, -1);
    EXPECT_EQ(desc.height, -1);
}

// -- All flags disabled --

TEST(WindowDescTest, AllFlagsDisabled) {
    WindowDesc desc;
    desc.vulkan = false;
    desc.hidpiEnabled = false;
    desc.resizable = false;
    desc.borderless = false;
    desc.fullscreen = false;
    desc.maximized = false;
    desc.hidden = false;

    uint64_t flags = desc.toSDLFlags();
    EXPECT_EQ(flags, 0u);
}

// -- All flags enabled --

TEST(WindowDescTest, AllFlagsEnabled) {
    WindowDesc desc;
    desc.vulkan = true;
    desc.hidpiEnabled = true;
    desc.resizable = true;
    desc.borderless = true;
    desc.fullscreen = true;
    desc.maximized = true;
    desc.hidden = true;

    uint64_t flags = desc.toSDLFlags();
    EXPECT_NE(flags & SDL_WINDOW_VULKAN, 0u);
    EXPECT_NE(flags & SDL_WINDOW_HIGH_PIXEL_DENSITY, 0u);
    EXPECT_NE(flags & SDL_WINDOW_RESIZABLE, 0u);
    EXPECT_NE(flags & SDL_WINDOW_BORDERLESS, 0u);
    EXPECT_NE(flags & SDL_WINDOW_FULLSCREEN, 0u);
    EXPECT_NE(flags & SDL_WINDOW_MAXIMIZED, 0u);
    EXPECT_NE(flags & SDL_WINDOW_HIDDEN, 0u);
}

// -- HiDPI flag isolation --

TEST(WindowDescTest, HiDPIDisabledFlagMapping) {
    WindowDesc desc;
    desc.hidpiEnabled = false;
    uint64_t flags = desc.toSDLFlags();
    EXPECT_EQ(flags & SDL_WINDOW_HIGH_PIXEL_DENSITY, 0u);
}

// -- fullscreenDesktop is not in toSDLFlags --

TEST(WindowDescTest, FullscreenDesktopNotInFlags) {
    WindowDesc desc;
    desc.fullscreenDesktop = true;
    desc.fullscreen = false;

    uint64_t flags = desc.toSDLFlags();
    // fullscreenDesktop is handled post-creation, not via flags
    EXPECT_EQ(flags & SDL_WINDOW_FULLSCREEN, 0u);
}

// -- fromConfig with empty table --

TEST(WindowDescTest, FromConfigEmptyTableRetainsAllDefaults) {
    auto table = toml::parse("");
    WindowDesc desc = windowDescFromConfig(table);

    EXPECT_EQ(desc.title, "Fabric");
    EXPECT_EQ(desc.width, 1280);
    EXPECT_EQ(desc.height, 720);
    EXPECT_EQ(desc.minWidth, 640);
    EXPECT_EQ(desc.minHeight, 480);
    EXPECT_EQ(desc.displayIndex, 0u);
    EXPECT_TRUE(desc.vulkan);
    EXPECT_TRUE(desc.hidpiEnabled);
    EXPECT_TRUE(desc.resizable);
    EXPECT_FALSE(desc.borderless);
    EXPECT_FALSE(desc.fullscreen);
    EXPECT_FALSE(desc.maximized);
}

// -- fromConfig with wrong value types --

TEST(WindowDescTest, FromConfigWrongTypeIgnored) {
    auto table = toml::parse(R"(
        width = "not_a_number"
        title = 42
        fullscreen = "yes"
    )");

    WindowDesc desc = windowDescFromConfig(table);

    // String where int expected: value<int64_t>() returns nullopt, default retained
    EXPECT_EQ(desc.width, 1280);
    // Int where string expected: value<std::string>() returns nullopt, default retained
    EXPECT_EQ(desc.title, "Fabric");
    // String where bool expected: value<bool>() returns nullopt, default retained
    EXPECT_FALSE(desc.fullscreen);
}

// -- fromConfig partial window table --

TEST(WindowDescTest, FromConfigPartialBooleans) {
    auto table = toml::parse(R"(
        fullscreen = true
        resizable = false
    )");

    WindowDesc desc = windowDescFromConfig(table);
    EXPECT_TRUE(desc.fullscreen);
    EXPECT_FALSE(desc.resizable);
    // Other booleans retain defaults
    EXPECT_FALSE(desc.borderless);
    EXPECT_TRUE(desc.hidpiEnabled);
}

// -- Conflicting flag combinations --

TEST(WindowDescTest, FullscreenAndBorderlessFlags) {
    WindowDesc desc;
    desc.fullscreen = true;
    desc.borderless = true;
    desc.maximized = true;

    uint64_t flags = desc.toSDLFlags();
    // All three flags should be present (SDL handles semantics)
    EXPECT_NE(flags & SDL_WINDOW_FULLSCREEN, 0u);
    EXPECT_NE(flags & SDL_WINDOW_BORDERLESS, 0u);
    EXPECT_NE(flags & SDL_WINDOW_MAXIMIZED, 0u);
}

TEST(WindowDescTest, HiddenAndMaximizedFlags) {
    WindowDesc desc;
    desc.hidden = true;
    desc.maximized = true;

    uint64_t flags = desc.toSDLFlags();
    EXPECT_NE(flags & SDL_WINDOW_HIDDEN, 0u);
    EXPECT_NE(flags & SDL_WINDOW_MAXIMIZED, 0u);
}

// -- Position sentinel values --

TEST(WindowDescTest, PositionSentinelValues) {
    WindowDesc desc;
    // Default -1 means centered
    EXPECT_EQ(desc.posX, -1);
    EXPECT_EQ(desc.posY, -1);

    desc.posX = 100;
    desc.posY = 200;
    EXPECT_EQ(desc.posX, 100);
    EXPECT_EQ(desc.posY, 200);
}

// -- Display index assignment --

TEST(WindowDescTest, DisplayIndexFromConfig) {
    auto table = toml::parse("display = 2");
    WindowDesc desc = windowDescFromConfig(table);
    EXPECT_EQ(desc.displayIndex, 2u);
}

// -- fullscreenDesktop from TOML config --

TEST(WindowDescTest, FullscreenDesktopFromConfig) {
    auto table = toml::parse("fullscreen_desktop = true");
    WindowDesc desc = windowDescFromConfig(table);
    EXPECT_TRUE(desc.fullscreenDesktop);
}

TEST(WindowDescTest, FullscreenDesktopDefaultFalseFromConfig) {
    auto table = toml::parse("");
    WindowDesc desc = windowDescFromConfig(table);
    EXPECT_FALSE(desc.fullscreenDesktop);
}

// -- hidden from TOML config --

TEST(WindowDescTest, HiddenFromConfig) {
    auto table = toml::parse("hidden = true");
    WindowDesc desc = windowDescFromConfig(table);
    EXPECT_TRUE(desc.hidden);
}
