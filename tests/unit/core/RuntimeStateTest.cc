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

// -- Boundary values --

TEST(RuntimeStateTest, MaxUint32WindowDimensions) {
    RuntimeState state;
    state.pixelWidth = UINT32_MAX;
    state.pixelHeight = UINT32_MAX;
    EXPECT_EQ(state.pixelWidth, UINT32_MAX);
    EXPECT_EQ(state.pixelHeight, UINT32_MAX);
}

TEST(RuntimeStateTest, ZeroWindowDimensions) {
    RuntimeState state;
    // Default is zero, verify it reads as zero (not uninitialized)
    EXPECT_EQ(state.pixelWidth, 0u);
    EXPECT_EQ(state.pixelHeight, 0u);
}

TEST(RuntimeStateTest, NegativePerformanceCounters) {
    RuntimeState state;
    state.fps = -1.0f;
    state.frameTimeMs = -0.001f;
    state.gpuTimeMs = -100.0f;
    state.drawCallCount = -1;

    EXPECT_FLOAT_EQ(state.fps, -1.0f);
    EXPECT_FLOAT_EQ(state.frameTimeMs, -0.001f);
    EXPECT_FLOAT_EQ(state.gpuTimeMs, -100.0f);
    EXPECT_EQ(state.drawCallCount, -1);
}

TEST(RuntimeStateTest, LargeSubsystemCounters) {
    RuntimeState state;
    state.physicsBodyCount = INT32_MAX;
    state.totalChunks = INT32_MAX;
    state.meshQueueSize = INT32_MAX;
    state.vramUsageMB = 16384.0f; // 16 GB

    EXPECT_EQ(state.physicsBodyCount, INT32_MAX);
    EXPECT_EQ(state.totalChunks, INT32_MAX);
    EXPECT_EQ(state.meshQueueSize, INT32_MAX);
    EXPECT_FLOAT_EQ(state.vramUsageMB, 16384.0f);
}

TEST(RuntimeStateTest, EmptyCurrentMode) {
    RuntimeState state;
    state.currentMode = "";
    EXPECT_TRUE(state.currentMode.empty());
}

TEST(RuntimeStateTest, AllDefaultFieldsCovered) {
    RuntimeState state;
    // Fields not checked in the original DefaultValues test
    EXPECT_FLOAT_EQ(state.gpuTimeMs, 0.0f);
    EXPECT_EQ(state.physicsBodyCount, 0);
    EXPECT_EQ(state.audioVoiceCount, 0);
    EXPECT_EQ(state.visibleChunks, 0);
    EXPECT_EQ(state.totalChunks, 0);
    EXPECT_EQ(state.meshQueueSize, 0);
    EXPECT_FLOAT_EQ(state.vramUsageMB, 0.0f);
}

TEST(RuntimeStateTest, DpiScaleBoundary) {
    RuntimeState state;
    state.dpiScale = 0.0f;
    EXPECT_FLOAT_EQ(state.dpiScale, 0.0f);

    state.dpiScale = 4.0f; // 4x Retina
    EXPECT_FLOAT_EQ(state.dpiScale, 4.0f);
}

TEST(RuntimeStateTest, ResetFlagsMaxValue) {
    RuntimeState state;
    state.resetFlags = UINT32_MAX;
    EXPECT_EQ(state.resetFlags, UINT32_MAX);
}

} // namespace fabric
