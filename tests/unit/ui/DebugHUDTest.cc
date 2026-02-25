#include "fabric/ui/DebugHUD.hh"
#include <gtest/gtest.h>

using namespace fabric;

TEST(DebugDataTest, DefaultInitialization) {
    DebugData data;
    EXPECT_FLOAT_EQ(data.fps, 0.0f);
    EXPECT_FLOAT_EQ(data.frameTimeMs, 0.0f);
    EXPECT_EQ(data.entityCount, 0);
    EXPECT_EQ(data.visibleChunks, 0);
    EXPECT_EQ(data.totalChunks, 0);
    EXPECT_EQ(data.triangleCount, 0);
    EXPECT_FLOAT_EQ(data.cameraPosition.x, 0.0f);
    EXPECT_FLOAT_EQ(data.cameraPosition.y, 0.0f);
    EXPECT_FLOAT_EQ(data.cameraPosition.z, 0.0f);
    EXPECT_EQ(data.currentRadius, 0);
    EXPECT_EQ(data.currentState, "None");
}

TEST(DebugDataTest, AssignValues) {
    DebugData data;
    data.fps = 60.0f;
    data.frameTimeMs = 16.67f;
    data.entityCount = 1000;
    data.visibleChunks = 42;
    data.totalChunks = 128;
    data.triangleCount = 500000;
    data.cameraPosition = {10.0f, 20.0f, 30.0f};
    data.currentRadius = 8;
    data.currentState = "Grounded";

    EXPECT_FLOAT_EQ(data.fps, 60.0f);
    EXPECT_EQ(data.entityCount, 1000);
    EXPECT_EQ(data.currentState, "Grounded");
}

TEST(DebugHUDTest, DefaultNotVisible) {
    DebugHUD hud;
    EXPECT_FALSE(hud.isVisible());
}

TEST(DebugHUDTest, ToggleChangesVisibility) {
    DebugHUD hud;
    EXPECT_FALSE(hud.isVisible());
    hud.toggle();
    EXPECT_TRUE(hud.isVisible());
    hud.toggle();
    EXPECT_FALSE(hud.isVisible());
}

TEST(DebugHUDTest, UpdateWithoutInitDoesNotCrash) {
    DebugHUD hud;
    DebugData data;
    data.fps = 60.0f;
    hud.update(data);
    // No crash, no-op
}

TEST(DebugHUDTest, InitWithNullContextDoesNotCrash) {
    DebugHUD hud;
    hud.init(nullptr);
    EXPECT_FALSE(hud.isVisible());
}

TEST(DebugHUDTest, ShutdownWithoutInitDoesNotCrash) {
    DebugHUD hud;
    hud.shutdown();
    EXPECT_FALSE(hud.isVisible());
}
