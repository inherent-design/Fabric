#include <gtest/gtest.h>

#include "fabric/core/RuntimeState.hh"

namespace fabric {

TEST(RuntimeStateTest, DefaultValues) {
    RuntimeState state;
    EXPECT_EQ(state.pixelWidth, 0u);
    EXPECT_EQ(state.pixelHeight, 0u);
    EXPECT_FLOAT_EQ(state.dpiScale, 1.0f);
    EXPECT_FALSE(state.fullscreen);
    EXPECT_FALSE(state.debugOverlay);
    EXPECT_FALSE(state.wireframe);
    EXPECT_EQ(state.resetFlags, 0u);
    EXPECT_EQ(state.currentMode, "Game");
    EXPECT_FALSE(state.simulationPaused);
    EXPECT_TRUE(state.mouseCaptured);
    EXPECT_FLOAT_EQ(state.fps, 0.0f);
    EXPECT_FLOAT_EQ(state.frameTimeMs, 0.0f);
    EXPECT_EQ(state.drawCallCount, 0);
}

TEST(RuntimeStateTest, SetAndRead) {
    RuntimeState state;

    state.pixelWidth = 1920;
    state.pixelHeight = 1080;
    state.dpiScale = 2.0f;
    state.fullscreen = true;
    state.currentMode = "Paused";
    state.fps = 60.0f;

    EXPECT_EQ(state.pixelWidth, 1920u);
    EXPECT_EQ(state.pixelHeight, 1080u);
    EXPECT_FLOAT_EQ(state.dpiScale, 2.0f);
    EXPECT_TRUE(state.fullscreen);
    EXPECT_EQ(state.currentMode, "Paused");
    EXPECT_FLOAT_EQ(state.fps, 60.0f);
}

TEST(RuntimeStateTest, SubsystemCounters) {
    RuntimeState state;

    state.physicsBodyCount = 128;
    state.audioVoiceCount = 8;
    state.visibleChunks = 50;
    state.totalChunks = 200;
    state.meshQueueSize = 12;
    state.vramUsageMB = 256.5f;

    EXPECT_EQ(state.physicsBodyCount, 128);
    EXPECT_EQ(state.audioVoiceCount, 8);
    EXPECT_EQ(state.visibleChunks, 50);
    EXPECT_EQ(state.totalChunks, 200);
    EXPECT_EQ(state.meshQueueSize, 12);
    EXPECT_FLOAT_EQ(state.vramUsageMB, 256.5f);
}

} // namespace fabric
