#include "fabric/platform/PlatformInfo.hh"
#include "fixtures/SDLFixture.hh"

#include <gtest/gtest.h>

using namespace fabric;
using fabric::test::SDLFixture;

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

TEST_F(SDLFixture, PopulateReturnsValidPlatformInfo) {
    PlatformInfo info;
    info.populate();

    // OS fields should be populated on any machine with SDL available
    EXPECT_FALSE(info.os.name.empty());
    EXPECT_GT(info.os.cpuCount, 0);
    EXPECT_GT(info.os.systemRAM, 0);

    // At least one display should be present when SDL video succeeds
    EXPECT_GE(info.displayCount(), 1u);

    const DisplayInfo* primary = info.primaryDisplay();
    ASSERT_NE(primary, nullptr);
    EXPECT_GT(primary->width, 0);
    EXPECT_GT(primary->height, 0);
}

// -- Multiple displays with no primary flag --

TEST(PlatformInfoTest, NoPrimaryFlagFallsBackToFirst) {
    PlatformInfo info;
    info.displays.push_back({1, "Left", 0, 0, 1920, 1080, 1.0f, 60.0f, false});
    info.displays.push_back({2, "Right", 1920, 0, 1920, 1080, 1.0f, 60.0f, false});
    info.displays.push_back({3, "Top", 0, -1080, 1920, 1080, 1.0f, 60.0f, false});

    const DisplayInfo* primary = info.primaryDisplay();
    ASSERT_NE(primary, nullptr);
    EXPECT_EQ(primary->id, 1u);
}

// -- Multiple primary displays (first flagged wins) --

TEST(PlatformInfoTest, MultiplePrimaryFlagsReturnsFirst) {
    PlatformInfo info;
    info.displays.push_back({1, "A", 0, 0, 1920, 1080, 1.0f, 60.0f, true});
    info.displays.push_back({2, "B", 1920, 0, 2560, 1440, 2.0f, 144.0f, true});

    const DisplayInfo* primary = info.primaryDisplay();
    ASSERT_NE(primary, nullptr);
    EXPECT_EQ(primary->id, 1u);
}

// -- Empty device lists for all types --

TEST(PlatformInfoTest, DevicesOfTypeEmptyForAllTypes) {
    PlatformInfo info;
    EXPECT_TRUE(info.devicesOfType(InputDeviceType::Keyboard).empty());
    EXPECT_TRUE(info.devicesOfType(InputDeviceType::Mouse).empty());
    EXPECT_TRUE(info.devicesOfType(InputDeviceType::Gamepad).empty());
    EXPECT_TRUE(info.devicesOfType(InputDeviceType::Touch).empty());
    EXPECT_TRUE(info.devicesOfType(InputDeviceType::Pen).empty());
}

// -- Pen device type --

TEST(PlatformInfoTest, DevicesOfTypePen) {
    PlatformInfo info;
    info.inputDevices.push_back({1, InputDeviceType::Pen, "Wacom"});
    info.inputDevices.push_back({2, InputDeviceType::Mouse, "Mouse"});

    auto pens = info.devicesOfType(InputDeviceType::Pen);
    EXPECT_EQ(pens.size(), 1u);
    EXPECT_EQ(pens[0].name, "Wacom");
}

// -- Multiple devices of same type --

TEST(PlatformInfoTest, MultipleDevicesSameType) {
    PlatformInfo info;
    info.inputDevices.push_back({1, InputDeviceType::Gamepad, "Xbox"});
    info.inputDevices.push_back({2, InputDeviceType::Gamepad, "PS5"});
    info.inputDevices.push_back({3, InputDeviceType::Gamepad, "Switch Pro"});

    auto gamepads = info.devicesOfType(InputDeviceType::Gamepad);
    EXPECT_EQ(gamepads.size(), 3u);
    EXPECT_TRUE(info.hasGamepad());
}

// -- hasGamepad / hasTouch with mixed devices --

TEST(PlatformInfoTest, HasGamepadWithMixedDevices) {
    PlatformInfo info;
    info.inputDevices.push_back({1, InputDeviceType::Keyboard, "KB"});
    info.inputDevices.push_back({2, InputDeviceType::Mouse, "Mouse"});
    EXPECT_FALSE(info.hasGamepad());
    EXPECT_FALSE(info.hasTouch());

    info.inputDevices.push_back({3, InputDeviceType::Gamepad, "Pad"});
    EXPECT_TRUE(info.hasGamepad());
    EXPECT_FALSE(info.hasTouch());
}

// -- GPUInfo field assignment --

TEST(PlatformInfoTest, GPUInfoFieldAssignment) {
    PlatformInfo info;
    info.gpu.vendorId = 0x10de;
    info.gpu.deviceId = 0x2684;
    info.gpu.vendorName = "NVIDIA";
    info.gpu.deviceName = "RTX 4090";
    info.gpu.driverInfo = "Vulkan";

    EXPECT_EQ(info.gpu.vendorId, 0x10de);
    EXPECT_EQ(info.gpu.deviceId, 0x2684);
    EXPECT_EQ(info.gpu.vendorName, "NVIDIA");
    EXPECT_EQ(info.gpu.deviceName, "RTX 4090");
    EXPECT_EQ(info.gpu.driverInfo, "Vulkan");
}

// -- OSInfo field assignment --

TEST(PlatformInfoTest, OSInfoFieldAssignment) {
    PlatformInfo info;
    info.os.name = "macOS";
    info.os.version = "3.2.0";
    info.os.arch = "arm64";
    info.os.cpuCount = 10;
    info.os.systemRAM = 32768;

    EXPECT_EQ(info.os.name, "macOS");
    EXPECT_EQ(info.os.version, "3.2.0");
    EXPECT_EQ(info.os.arch, "arm64");
    EXPECT_EQ(info.os.cpuCount, 10);
    EXPECT_EQ(info.os.systemRAM, 32768);
}

// -- DisplayInfo defaults --

TEST(PlatformInfoTest, DisplayInfoDefaultConstruction) {
    DisplayInfo d;
    EXPECT_EQ(d.id, 0u);
    EXPECT_TRUE(d.name.empty());
    EXPECT_EQ(d.x, 0);
    EXPECT_EQ(d.y, 0);
    EXPECT_EQ(d.width, 0);
    EXPECT_EQ(d.height, 0);
    EXPECT_FLOAT_EQ(d.contentScale, 1.0f);
    EXPECT_FLOAT_EQ(d.refreshRate, 0.0f);
    EXPECT_FALSE(d.primary);
}

// -- InputDeviceInfo defaults --

TEST(PlatformInfoTest, InputDeviceInfoDefaultConstruction) {
    InputDeviceInfo dev;
    EXPECT_EQ(dev.id, 0u);
    EXPECT_EQ(dev.type, InputDeviceType::Keyboard);
    EXPECT_TRUE(dev.name.empty());
}

// -- PlatformDirs default construction --

TEST(PlatformInfoTest, PlatformDirsDefaultConstruction) {
    PlatformDirs dirs;
    EXPECT_TRUE(dirs.basePath.empty());
    EXPECT_TRUE(dirs.prefPath.empty());
    EXPECT_TRUE(dirs.configDir.empty());
    EXPECT_TRUE(dirs.cacheDir.empty());
    EXPECT_TRUE(dirs.dataDir.empty());
}

// -- Vulkan API version --

// ── vendorNameFromId ───────────────────────────────────────────────────

TEST(PlatformInfoTest, VendorNameFromIdApple) {
    EXPECT_EQ(vendorNameFromId(0x106b), "Apple");
}

TEST(PlatformInfoTest, VendorNameFromIdKnownVendors) {
    EXPECT_EQ(vendorNameFromId(0x10de), "NVIDIA");
    EXPECT_EQ(vendorNameFromId(0x1002), "AMD");
    EXPECT_EQ(vendorNameFromId(0x8086), "Intel");
    EXPECT_EQ(vendorNameFromId(0x13b5), "ARM");
}

TEST(PlatformInfoTest, VendorNameFromIdUnknown) {
    EXPECT_EQ(vendorNameFromId(0xFFFF), "Unknown");
    EXPECT_EQ(vendorNameFromId(0x0000), "Unknown");
}

// ── Vulkan API version ────────────────────────────────────────────────

TEST(PlatformInfoTest, VulkanApiVersionStorage) {
    PlatformInfo info;
    // VK_MAKE_VERSION(1, 3, 0) = (1 << 22) | (3 << 12) | 0
    uint32_t version = (1u << 22) | (3u << 12) | 0u;
    info.vulkanApiVersion = version;
    EXPECT_EQ(info.vulkanApiVersion, version);
}
