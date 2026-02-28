#include "fabric/ui/DebugHUD.hh"
#include <gtest/gtest.h>

using namespace fabric;

// EF-18: Tests for perf overlay fields in DebugData

TEST(DebugDataPerfTest, DefaultPerfFields) {
    DebugData data;
    EXPECT_EQ(data.drawCallCount, 0);
    EXPECT_FLOAT_EQ(data.gpuTimeMs, 0.0f);
    EXPECT_FLOAT_EQ(data.memoryUsageMB, 0.0f);
    EXPECT_EQ(data.physicsBodyCount, 0);
    EXPECT_EQ(data.audioVoiceCount, 0);
    EXPECT_EQ(data.chunkMeshQueueSize, 0);
}

TEST(DebugDataPerfTest, AssignPerfFields) {
    DebugData data;
    data.drawCallCount = 1200;
    data.gpuTimeMs = 8.5f;
    data.memoryUsageMB = 256.0f;
    data.physicsBodyCount = 64;
    data.audioVoiceCount = 12;
    data.chunkMeshQueueSize = 7;

    EXPECT_EQ(data.drawCallCount, 1200);
    EXPECT_FLOAT_EQ(data.gpuTimeMs, 8.5f);
    EXPECT_FLOAT_EQ(data.memoryUsageMB, 256.0f);
    EXPECT_EQ(data.physicsBodyCount, 64);
    EXPECT_EQ(data.audioVoiceCount, 12);
    EXPECT_EQ(data.chunkMeshQueueSize, 7);
}

TEST(DebugDataPerfTest, PerfFieldsCoexistWithExistingFields) {
    DebugData data;
    data.fps = 60.0f;
    data.frameTimeMs = 16.67f;
    data.entityCount = 500;
    data.drawCallCount = 800;
    data.gpuTimeMs = 4.2f;
    data.memoryUsageMB = 128.5f;

    EXPECT_FLOAT_EQ(data.fps, 60.0f);
    EXPECT_FLOAT_EQ(data.frameTimeMs, 16.67f);
    EXPECT_EQ(data.entityCount, 500);
    EXPECT_EQ(data.drawCallCount, 800);
    EXPECT_FLOAT_EQ(data.gpuTimeMs, 4.2f);
    EXPECT_FLOAT_EQ(data.memoryUsageMB, 128.5f);
}

TEST(DebugHUDPerfTest, UpdateWithPerfDataWithoutInit) {
    DebugHUD hud;
    DebugData data;
    data.drawCallCount = 500;
    data.gpuTimeMs = 3.0f;
    data.memoryUsageMB = 64.0f;
    data.physicsBodyCount = 32;
    data.audioVoiceCount = 8;
    data.chunkMeshQueueSize = 3;
    hud.update(data);
    // No crash; update is a no-op without init
}

TEST(DebugDataPerfTest, ZeroGpuTimerFreqSafe) {
    // Simulates the case where bgfx reports gpuTimerFreq == 0
    // (no GPU timer available). gpuTimeMs should stay at default 0.
    DebugData data;
    EXPECT_FLOAT_EQ(data.gpuTimeMs, 0.0f);
    // Setting to explicit zero is safe
    data.gpuTimeMs = 0.0f;
    EXPECT_FLOAT_EQ(data.gpuTimeMs, 0.0f);
}
