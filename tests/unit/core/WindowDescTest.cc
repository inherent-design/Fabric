#include "fabric/platform/WindowDesc.hh"

#include <gtest/gtest.h>

#include <SDL3/SDL.h>
#include <toml++/toml.hpp>

using namespace fabric;

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

TEST(WindowDescTest, CreateWindowRequiresSDLRuntime) {
    GTEST_SKIP() << "Requires live SDL runtime to validate createWindow().";
}
