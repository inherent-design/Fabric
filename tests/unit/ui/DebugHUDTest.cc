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

    // Profiler metrics default to zero
    EXPECT_EQ(data.drawCallCount, 0);
    EXPECT_FLOAT_EQ(data.gpuTimeMs, 0.0f);
    EXPECT_FLOAT_EQ(data.memoryUsageMB, 0.0f);
    EXPECT_EQ(data.physicsBodyCount, 0);
    EXPECT_EQ(data.audioVoiceCount, 0);
    EXPECT_EQ(data.chunkMeshQueueSize, 0);
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

TEST(DebugDataTest, ProfilerMetricsAssignment) {
    DebugData data;
    data.drawCallCount = 256;
    data.gpuTimeMs = 8.45f;
    data.memoryUsageMB = 512.75f;
    data.physicsBodyCount = 64;
    data.audioVoiceCount = 12;
    data.chunkMeshQueueSize = 7;

    EXPECT_EQ(data.drawCallCount, 256);
    EXPECT_FLOAT_EQ(data.gpuTimeMs, 8.45f);
    EXPECT_FLOAT_EQ(data.memoryUsageMB, 512.75f);
    EXPECT_EQ(data.physicsBodyCount, 64);
    EXPECT_EQ(data.audioVoiceCount, 12);
    EXPECT_EQ(data.chunkMeshQueueSize, 7);
}

TEST(DebugDataTest, ProfilerMetricsRoundTripThroughUpdate) {
    // Verify data survives struct copy (same pattern as update() uses)
    DebugData source;
    source.fps = 144.0f;
    source.drawCallCount = 512;
    source.gpuTimeMs = 3.2f;
    source.memoryUsageMB = 1024.0f;
    source.physicsBodyCount = 128;
    source.audioVoiceCount = 32;
    source.chunkMeshQueueSize = 15;

    DebugData dest = source;

    EXPECT_FLOAT_EQ(dest.fps, 144.0f);
    EXPECT_EQ(dest.drawCallCount, 512);
    EXPECT_FLOAT_EQ(dest.gpuTimeMs, 3.2f);
    EXPECT_FLOAT_EQ(dest.memoryUsageMB, 1024.0f);
    EXPECT_EQ(dest.physicsBodyCount, 128);
    EXPECT_EQ(dest.audioVoiceCount, 32);
    EXPECT_EQ(dest.chunkMeshQueueSize, 15);
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
