#include "fabric/platform/PlatformInfo.hh"

#include <gtest/gtest.h>

using namespace fabric;

TEST(PlatformInfoTest, DefaultState) {
    PlatformInfo info;
    EXPECT_TRUE(info.os.name.empty());
    EXPECT_EQ(info.os.cpuCount, 0);
    EXPECT_EQ(info.os.systemRAM, 0);
    EXPECT_EQ(info.gpu.vendorId, 0);
    EXPECT_EQ(info.gpu.deviceId, 0);
    EXPECT_TRUE(info.displays.empty());
    EXPECT_TRUE(info.inputDevices.empty());
    EXPECT_EQ(info.vulkanApiVersion, 0u);
}

TEST(PlatformInfoTest, PrimaryDisplayReturnsNullWhenEmpty) {
    PlatformInfo info;
    EXPECT_EQ(info.primaryDisplay(), nullptr);
}

TEST(PlatformInfoTest, PrimaryDisplayReturnsFlaggedDisplay) {
    PlatformInfo info;
    info.displays.push_back({1, "Display 1", 0, 0, 1920, 1080, 1.0f, 60.0f, false});
    info.displays.push_back({2, "Display 2", 1920, 0, 2560, 1440, 2.0f, 144.0f, true});

    const DisplayInfo* primary = info.primaryDisplay();
    ASSERT_NE(primary, nullptr);
    EXPECT_EQ(primary->id, 2u);
    EXPECT_TRUE(primary->primary);
}

TEST(PlatformInfoTest, PrimaryDisplayFallsBackToFirst) {
    PlatformInfo info;
    info.displays.push_back({1, "Display 1", 0, 0, 1920, 1080, 1.0f, 60.0f, false});

    const DisplayInfo* primary = info.primaryDisplay();
    ASSERT_NE(primary, nullptr);
    EXPECT_EQ(primary->id, 1u);
}

TEST(PlatformInfoTest, DisplayCount) {
    PlatformInfo info;
    EXPECT_EQ(info.displayCount(), 0u);

    info.displays.push_back({1, "A", 0, 0, 1920, 1080, 1.0f, 60.0f, true});
    info.displays.push_back({2, "B", 1920, 0, 1920, 1080, 1.0f, 60.0f, false});
    EXPECT_EQ(info.displayCount(), 2u);
}

TEST(PlatformInfoTest, DevicesOfTypeFiltersCorrectly) {
    PlatformInfo info;
    info.inputDevices.push_back({1, InputDeviceType::Keyboard, "KB1"});
    info.inputDevices.push_back({2, InputDeviceType::Mouse, "Mouse1"});
    info.inputDevices.push_back({3, InputDeviceType::Gamepad, "Gamepad1"});
    info.inputDevices.push_back({4, InputDeviceType::Keyboard, "KB2"});

    auto keyboards = info.devicesOfType(InputDeviceType::Keyboard);
    EXPECT_EQ(keyboards.size(), 2u);

    auto gamepads = info.devicesOfType(InputDeviceType::Gamepad);
    EXPECT_EQ(gamepads.size(), 1u);
    EXPECT_EQ(gamepads[0].name, "Gamepad1");

    auto touches = info.devicesOfType(InputDeviceType::Touch);
    EXPECT_TRUE(touches.empty());
}

TEST(PlatformInfoTest, HasGamepad) {
    PlatformInfo info;
    EXPECT_FALSE(info.hasGamepad());

    info.inputDevices.push_back({1, InputDeviceType::Gamepad, "Pad"});
    EXPECT_TRUE(info.hasGamepad());
}

TEST(PlatformInfoTest, HasTouch) {
    PlatformInfo info;
    EXPECT_FALSE(info.hasTouch());

    info.inputDevices.push_back({1, InputDeviceType::Touch, "Touch"});
    EXPECT_TRUE(info.hasTouch());
}

TEST(PlatformInfoTest, PlatformDirsFieldAssignment) {
    PlatformDirs dirs;
    dirs.basePath = "/usr/bin/";
    dirs.prefPath = "/home/user/.local/share/fabric/fabric/";
    dirs.configDir = "/home/user/.config/fabric";
    dirs.cacheDir = "/home/user/.cache/fabric";
    dirs.dataDir = "/home/user/.local/share/fabric";

    PlatformInfo info;
    info.dirs = dirs;

    EXPECT_EQ(info.dirs.basePath, "/usr/bin/");
    EXPECT_EQ(info.dirs.configDir, "/home/user/.config/fabric");
    EXPECT_EQ(info.dirs.cacheDir, "/home/user/.cache/fabric");
    EXPECT_EQ(info.dirs.dataDir, "/home/user/.local/share/fabric");
}

TEST(PlatformInfoTest, PopulateRequiresSDLRuntime) {
    GTEST_SKIP() << "Requires live SDL runtime to validate populate().";
}
