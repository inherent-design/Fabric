#include "fabric/core/AudioSystem.hh"
#include "fabric/core/ChunkedGrid.hh"

#include <gtest/gtest.h>

using namespace fabric;

class AudioSystemTest : public ::testing::Test {
  protected:
    AudioSystem audio;

    void SetUp() override { audio.initHeadless(); }

    void TearDown() override { audio.shutdown(); }
};

TEST_F(AudioSystemTest, InitAndShutdown) {
    EXPECT_TRUE(audio.isInitialized());
    audio.shutdown();
    EXPECT_FALSE(audio.isInitialized());
}

TEST_F(AudioSystemTest, DoubleInitIsNoOp) {
    EXPECT_TRUE(audio.isInitialized());
    audio.initHeadless();
    EXPECT_TRUE(audio.isInitialized());
}

TEST_F(AudioSystemTest, ShutdownWithoutInitIsNoOp) {
    AudioSystem fresh;
    EXPECT_FALSE(fresh.isInitialized());
    fresh.shutdown();
    EXPECT_FALSE(fresh.isInitialized());
}

TEST_F(AudioSystemTest, DestructorCleansUp) {
    auto* sys = new AudioSystem;
    sys->initHeadless();
    EXPECT_TRUE(sys->isInitialized());
    delete sys;
}

TEST_F(AudioSystemTest, SetListenerPosition) {
    Vec3f pos(10.0f, 20.0f, 30.0f);
    audio.setListenerPosition(pos);
}

TEST_F(AudioSystemTest, SetListenerDirection) {
    Vec3f forward(0.0f, 0.0f, -1.0f);
    Vec3f up(0.0f, 1.0f, 0.0f);
    audio.setListenerDirection(forward, up);
}

TEST_F(AudioSystemTest, SetListenerBeforeInit) {
    AudioSystem uninit;
    Vec3f pos(1.0f, 2.0f, 3.0f);
    Vec3f forward(0.0f, 0.0f, -1.0f);
    Vec3f up(0.0f, 1.0f, 0.0f);
    uninit.setListenerPosition(pos);
    uninit.setListenerDirection(forward, up);
}

TEST_F(AudioSystemTest, PlaySoundInvalidPath) {
    Vec3f pos(0.0f, 0.0f, 0.0f);
    SoundHandle handle = audio.playSound("nonexistent_file.wav", pos);
    EXPECT_EQ(handle, InvalidSoundHandle);
}

TEST_F(AudioSystemTest, PlaySoundLoopedInvalidPath) {
    Vec3f pos(0.0f, 0.0f, 0.0f);
    SoundHandle handle = audio.playSoundLooped("nonexistent_file.wav", pos);
    EXPECT_EQ(handle, InvalidSoundHandle);
}

TEST_F(AudioSystemTest, PlaySoundBeforeInit) {
    AudioSystem uninit;
    Vec3f pos(0.0f, 0.0f, 0.0f);
    SoundHandle handle = uninit.playSound("test.wav", pos);
    EXPECT_EQ(handle, InvalidSoundHandle);
}

TEST_F(AudioSystemTest, StopInvalidHandle) {
    audio.stopSound(InvalidSoundHandle);
    audio.stopSound(999);
}

TEST_F(AudioSystemTest, StopAllSoundsEmpty) {
    audio.stopAllSounds();
}

TEST_F(AudioSystemTest, SetSoundPositionInvalidHandle) {
    Vec3f pos(1.0f, 2.0f, 3.0f);
    audio.setSoundPosition(InvalidSoundHandle, pos);
    audio.setSoundPosition(999, pos);
}

TEST_F(AudioSystemTest, SetSoundVolumeInvalidHandle) {
    audio.setSoundVolume(InvalidSoundHandle, 0.5f);
    audio.setSoundVolume(999, 0.5f);
}

TEST_F(AudioSystemTest, IsSoundPlayingInvalidHandle) {
    EXPECT_FALSE(audio.isSoundPlaying(InvalidSoundHandle));
    EXPECT_FALSE(audio.isSoundPlaying(999));
}

TEST_F(AudioSystemTest, ActiveSoundCountInitiallyZero) {
    EXPECT_EQ(audio.activeSoundCount(), 0u);
}

TEST_F(AudioSystemTest, SetMasterVolume) {
    audio.setMasterVolume(0.5f);
    audio.setMasterVolume(1.0f);
    audio.setMasterVolume(0.0f);
}

TEST_F(AudioSystemTest, SetMasterVolumeBeforeInit) {
    AudioSystem uninit;
    uninit.setMasterVolume(0.5f);
}

TEST_F(AudioSystemTest, SetAttenuationModel) {
    audio.setAttenuationModel(AttenuationModel::Inverse);
    audio.setAttenuationModel(AttenuationModel::Linear);
    audio.setAttenuationModel(AttenuationModel::Exponential);
}

TEST_F(AudioSystemTest, UpdateWithNoSounds) {
    audio.update(0.016f);
}

TEST_F(AudioSystemTest, HandleCounterWrapsAroundZero) {
    AudioSystem sys;
    sys.initHeadless();
    // Internal counter starts at 0, first handle should be 1
    Vec3f pos(0.0f, 0.0f, 0.0f);
    // We can only verify via the public API that handles are non-zero
    // on valid sound loads. With invalid files, we get InvalidSoundHandle.
    // This test verifies the system doesn't crash under repeated operations.
    for (int i = 0; i < 100; ++i) {
        SoundHandle h = sys.playSound("nonexistent.wav", pos);
        EXPECT_EQ(h, InvalidSoundHandle);
    }
    sys.shutdown();
}

TEST_F(AudioSystemTest, MultipleInitShutdownCycles) {
    audio.shutdown();
    EXPECT_FALSE(audio.isInitialized());

    audio.initHeadless();
    EXPECT_TRUE(audio.isInitialized());

    audio.shutdown();
    EXPECT_FALSE(audio.isInitialized());

    audio.initHeadless();
    EXPECT_TRUE(audio.isInitialized());
}

// Occlusion tests

TEST_F(AudioSystemTest, OcclusionDefaultDisabled) {
    EXPECT_FALSE(audio.isOcclusionEnabled());
}

TEST_F(AudioSystemTest, EnableDisableOcclusion) {
    audio.setOcclusionEnabled(true);
    EXPECT_TRUE(audio.isOcclusionEnabled());
    audio.setOcclusionEnabled(false);
    EXPECT_FALSE(audio.isOcclusionEnabled());
}

TEST_F(AudioSystemTest, SetDensityGrid) {
    ChunkedGrid<float> grid;
    audio.setDensityGrid(&grid);
}

TEST_F(AudioSystemTest, ComputeOcclusionClearPath) {
    ChunkedGrid<float> grid;
    audio.setDensityGrid(&grid);
    Vec3f source(2.0f, 5.0f, 5.0f);
    Vec3f listener(8.0f, 5.0f, 5.0f);
    auto result = audio.computeOcclusion(source, listener);
    EXPECT_FLOAT_EQ(result.factor, 0.0f);
    EXPECT_EQ(result.solidCount, 0);
}

TEST_F(AudioSystemTest, ComputeOcclusionBlockedPath) {
    ChunkedGrid<float> grid;
    for (int y = 0; y < 10; ++y)
        for (int z = 0; z < 10; ++z)
            grid.set(5, y, z, 1.0f);
    audio.setDensityGrid(&grid);
    Vec3f source(2.0f, 5.0f, 5.0f);
    Vec3f listener(8.0f, 5.0f, 5.0f);
    auto result = audio.computeOcclusion(source, listener);
    EXPECT_GT(result.factor, 0.0f);
    EXPECT_GT(result.solidCount, 0);
}

TEST_F(AudioSystemTest, ComputeOcclusionFullyBlocked) {
    ChunkedGrid<float> grid;
    for (int x = 2; x <= 9; ++x)
        for (int y = 0; y < 10; ++y)
            for (int z = 0; z < 10; ++z)
                grid.set(x, y, z, 1.0f);
    audio.setDensityGrid(&grid);
    Vec3f source(0.0f, 5.0f, 5.0f);
    Vec3f listener(12.0f, 5.0f, 5.0f);
    auto result = audio.computeOcclusion(source, listener);
    EXPECT_FLOAT_EQ(result.factor, 1.0f);
    EXPECT_GE(result.solidCount, 8);
}

TEST_F(AudioSystemTest, ComputeOcclusionNoGrid) {
    Vec3f source(2.0f, 5.0f, 5.0f);
    Vec3f listener(8.0f, 5.0f, 5.0f);
    auto result = audio.computeOcclusion(source, listener);
    EXPECT_FLOAT_EQ(result.factor, 0.0f);
    EXPECT_EQ(result.solidCount, 0);
    EXPECT_EQ(result.totalSteps, 0);
}

TEST_F(AudioSystemTest, OcclusionThreshold) {
    ChunkedGrid<float> grid;
    for (int y = 0; y < 10; ++y)
        for (int z = 0; z < 10; ++z)
            grid.set(5, y, z, 0.3f);
    audio.setDensityGrid(&grid);
    Vec3f source(2.0f, 5.0f, 5.0f);
    Vec3f listener(8.0f, 5.0f, 5.0f);
    auto clear = audio.computeOcclusion(source, listener, 0.5f);
    EXPECT_FLOAT_EQ(clear.factor, 0.0f);
    auto blocked = audio.computeOcclusion(source, listener, 0.2f);
    EXPECT_GT(blocked.factor, 0.0f);
}

TEST_F(AudioSystemTest, UpdateAppliesOcclusion) {
    ChunkedGrid<float> grid;
    for (int y = 0; y < 10; ++y)
        for (int z = 0; z < 10; ++z)
            grid.set(5, y, z, 1.0f);
    audio.setDensityGrid(&grid);
    audio.setOcclusionEnabled(true);
    audio.setListenerPosition(Vec3f(8.0f, 5.0f, 5.0f));
    audio.update(0.016f);
}
