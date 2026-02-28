#include "fabric/core/AudioSystem.hh"
#include "fabric/core/ChunkedGrid.hh"
#include "fabric/core/ReverbZone.hh"

#include <gtest/gtest.h>

using namespace fabric;

class AudioSystemTest : public ::testing::Test {
  protected:
    AudioSystem audio;

    void SetUp() override {
        audio.initHeadless();
        audio.setCommandBufferEnabled(false);
    }

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

TEST_F(AudioSystemTest, SetAttenuationModelBeforeInit) {
    AudioSystem uninit;
    uninit.setAttenuationModel(AttenuationModel::Linear);
    uninit.setAttenuationModel(AttenuationModel::Exponential);
}

TEST_F(AudioSystemTest, AttenuationModelResetOnShutdown) {
    audio.setAttenuationModel(AttenuationModel::Exponential);
    audio.shutdown();
    audio.initHeadless();
    audio.setCommandBufferEnabled(false);
    // After shutdown/reinit, model should be back to default (Inverse).
    // Verify by calling setAttenuationModel again without crash.
    audio.setAttenuationModel(AttenuationModel::Linear);
}

TEST_F(AudioSystemTest, UpdateWithNoSounds) {
    audio.update(0.016f);
}

TEST_F(AudioSystemTest, HandleCounterWrapsAroundZero) {
    AudioSystem sys;
    sys.initHeadless();
    sys.setCommandBufferEnabled(false);
    Vec3f pos(0.0f, 0.0f, 0.0f);
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
    audio.setCommandBufferEnabled(false);
    EXPECT_TRUE(audio.isInitialized());

    audio.shutdown();
    EXPECT_FALSE(audio.isInitialized());

    audio.initHeadless();
    audio.setCommandBufferEnabled(false);
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

// --- SPSC Ring Buffer Tests ---

TEST(SPSCRingBufferTest, PushAndPop) {
    SPSCRingBuffer<int, 4> buf;
    EXPECT_EQ(buf.size(), 0u);

    EXPECT_TRUE(buf.tryPush(42));
    EXPECT_EQ(buf.size(), 1u);

    int val = 0;
    EXPECT_TRUE(buf.tryPop(val));
    EXPECT_EQ(val, 42);
    EXPECT_EQ(buf.size(), 0u);
}

TEST(SPSCRingBufferTest, PopEmptyReturnsFalse) {
    SPSCRingBuffer<int, 4> buf;
    int val = 0;
    EXPECT_FALSE(buf.tryPop(val));
}

TEST(SPSCRingBufferTest, PushFullReturnsFalse) {
    SPSCRingBuffer<int, 4> buf;
    for (int i = 0; i < 4; ++i) {
        EXPECT_TRUE(buf.tryPush(int{i}));
    }
    EXPECT_FALSE(buf.tryPush(int{99}));
    EXPECT_EQ(buf.size(), 4u);
}

TEST(SPSCRingBufferTest, FIFOOrdering) {
    SPSCRingBuffer<int, 8> buf;
    for (int i = 0; i < 5; ++i) {
        EXPECT_TRUE(buf.tryPush(int{i}));
    }
    for (int i = 0; i < 5; ++i) {
        int val = -1;
        EXPECT_TRUE(buf.tryPop(val));
        EXPECT_EQ(val, i);
    }
}

TEST(SPSCRingBufferTest, WrapAround) {
    SPSCRingBuffer<int, 4> buf;
    for (int round = 0; round < 3; ++round) {
        for (int i = 0; i < 4; ++i) {
            EXPECT_TRUE(buf.tryPush(int{round * 10 + i}));
        }
        for (int i = 0; i < 4; ++i) {
            int val = -1;
            EXPECT_TRUE(buf.tryPop(val));
            EXPECT_EQ(val, round * 10 + i);
        }
        EXPECT_EQ(buf.size(), 0u);
    }
}

TEST(SPSCRingBufferTest, MoveSemantics) {
    SPSCRingBuffer<std::string, 4> buf;
    std::string s = "hello";
    EXPECT_TRUE(buf.tryPush(std::move(s)));

    std::string out;
    EXPECT_TRUE(buf.tryPop(out));
    EXPECT_EQ(out, "hello");
}

TEST(SPSCRingBufferTest, SizeTracking) {
    SPSCRingBuffer<int, 8> buf;
    EXPECT_EQ(buf.size(), 0u);

    for (size_t i = 0; i < 5; ++i) {
        buf.tryPush(static_cast<int>(i));
        EXPECT_EQ(buf.size(), i + 1);
    }

    for (size_t i = 0; i < 3; ++i) {
        int val;
        buf.tryPop(val);
        EXPECT_EQ(buf.size(), 4 - i);
    }
}

// --- Command Buffer Tests ---

class AudioCommandBufferTest : public ::testing::Test {
  protected:
    AudioSystem audio;

    void SetUp() override {
        audio.initHeadless();
        // Command buffer enabled by default after init
    }

    void TearDown() override { audio.shutdown(); }
};

TEST_F(AudioCommandBufferTest, CommandBufferEnabledByDefault) {
    EXPECT_TRUE(audio.isCommandBufferEnabled());
}

TEST_F(AudioCommandBufferTest, ToggleCommandBuffer) {
    audio.setCommandBufferEnabled(false);
    EXPECT_FALSE(audio.isCommandBufferEnabled());
    audio.setCommandBufferEnabled(true);
    EXPECT_TRUE(audio.isCommandBufferEnabled());
}

TEST_F(AudioCommandBufferTest, PlaySoundReturnsDeferredHandle) {
    Vec3f pos(0.0f, 0.0f, 0.0f);
    SoundHandle handle = audio.playSound("nonexistent.wav", pos);
    // With command buffer, handle is pre-allocated before execution
    EXPECT_NE(handle, InvalidSoundHandle);
}

TEST_F(AudioCommandBufferTest, PlaySoundLoopedReturnsDeferredHandle) {
    Vec3f pos(0.0f, 0.0f, 0.0f);
    SoundHandle handle = audio.playSoundLooped("nonexistent.wav", pos);
    EXPECT_NE(handle, InvalidSoundHandle);
}

TEST_F(AudioCommandBufferTest, CommandsDrainedOnUpdate) {
    Vec3f pos(0.0f, 0.0f, 0.0f);
    audio.playSound("nonexistent.wav", pos);
    // Command queued but not executed yet, drain on update
    audio.update(0.016f);
}

TEST_F(AudioCommandBufferTest, PlaySoundBeforeInitReturnsInvalid) {
    AudioSystem uninit;
    Vec3f pos(0.0f, 0.0f, 0.0f);
    SoundHandle handle = uninit.playSound("test.wav", pos);
    EXPECT_EQ(handle, InvalidSoundHandle);
}

TEST_F(AudioCommandBufferTest, HandleIncrementsSequentially) {
    Vec3f pos(0.0f, 0.0f, 0.0f);
    SoundHandle h1 = audio.playSound("a.wav", pos);
    SoundHandle h2 = audio.playSound("b.wav", pos);
    SoundHandle h3 = audio.playSound("c.wav", pos);
    EXPECT_NE(h1, InvalidSoundHandle);
    EXPECT_NE(h2, InvalidSoundHandle);
    EXPECT_NE(h3, InvalidSoundHandle);
    EXPECT_LT(h1, h2);
    EXPECT_LT(h2, h3);
}

TEST_F(AudioCommandBufferTest, StopSoundQueuesCommand) {
    audio.stopSound(42);
    audio.update(0.016f);
}

TEST_F(AudioCommandBufferTest, StopAllSoundsQueuesCommand) {
    audio.stopAllSounds();
    audio.update(0.016f);
}

TEST_F(AudioCommandBufferTest, SetPositionQueuesCommand) {
    audio.setSoundPosition(1, Vec3f(1.0f, 2.0f, 3.0f));
    audio.update(0.016f);
}

TEST_F(AudioCommandBufferTest, SetVolumeQueuesCommand) {
    audio.setSoundVolume(1, 0.5f);
    audio.update(0.016f);
}

TEST_F(AudioCommandBufferTest, ListenerPositionQueuesCommand) {
    audio.setListenerPosition(Vec3f(5.0f, 0.0f, 0.0f));
    audio.update(0.016f);
}

TEST_F(AudioCommandBufferTest, ListenerDirectionQueuesCommand) {
    audio.setListenerDirection(Vec3f(0.0f, 0.0f, -1.0f), Vec3f(0.0f, 1.0f, 0.0f));
    audio.update(0.016f);
}

TEST_F(AudioCommandBufferTest, PlaySoundWithCategory) {
    Vec3f pos(0.0f, 0.0f, 0.0f);
    SoundHandle handle = audio.playSound("nonexistent.wav", pos, SoundCategory::Music);
    EXPECT_NE(handle, InvalidSoundHandle);
    audio.update(0.016f);
}

TEST_F(AudioCommandBufferTest, ShutdownDrainsBuffer) {
    Vec3f pos(0.0f, 0.0f, 0.0f);
    audio.playSound("nonexistent.wav", pos);
    audio.stopAllSounds();
    // shutdown should drain without crashing
    audio.shutdown();
    EXPECT_FALSE(audio.isInitialized());
}

// --- Sound Category Tests ---

TEST_F(AudioSystemTest, DefaultCategoryVolumes) {
    EXPECT_FLOAT_EQ(audio.getCategoryVolume(SoundCategory::Master), 1.0f);
    EXPECT_FLOAT_EQ(audio.getCategoryVolume(SoundCategory::SFX), 1.0f);
    EXPECT_FLOAT_EQ(audio.getCategoryVolume(SoundCategory::Music), 1.0f);
    EXPECT_FLOAT_EQ(audio.getCategoryVolume(SoundCategory::Ambient), 1.0f);
    EXPECT_FLOAT_EQ(audio.getCategoryVolume(SoundCategory::UI), 1.0f);
}

TEST_F(AudioSystemTest, SetCategoryVolume) {
    audio.setCategoryVolume(SoundCategory::SFX, 0.5f);
    EXPECT_FLOAT_EQ(audio.getCategoryVolume(SoundCategory::SFX), 0.5f);
    EXPECT_FLOAT_EQ(audio.getCategoryVolume(SoundCategory::Music), 1.0f);
}

TEST_F(AudioSystemTest, SetMasterCategoryVolume) {
    audio.setCategoryVolume(SoundCategory::Master, 0.7f);
    EXPECT_FLOAT_EQ(audio.getCategoryVolume(SoundCategory::Master), 0.7f);
}

TEST_F(AudioSystemTest, CategoryVolumeInvalidCategory) {
    EXPECT_FLOAT_EQ(audio.getCategoryVolume(SoundCategory::Count), 0.0f);
}

TEST_F(AudioSystemTest, SetCategoryVolumeInvalidCategory) {
    audio.setCategoryVolume(SoundCategory::Count, 0.5f);
    // Should not crash, silently ignored
}

TEST_F(AudioSystemTest, PlaySoundWithCategory) {
    Vec3f pos(0.0f, 0.0f, 0.0f);
    SoundHandle handle = audio.playSound("nonexistent.wav", pos, SoundCategory::Music);
    EXPECT_EQ(handle, InvalidSoundHandle);
}

TEST_F(AudioSystemTest, PlaySoundLoopedWithCategory) {
    Vec3f pos(0.0f, 0.0f, 0.0f);
    SoundHandle handle = audio.playSoundLooped("nonexistent.wav", pos, SoundCategory::Ambient);
    EXPECT_EQ(handle, InvalidSoundHandle);
}

TEST_F(AudioSystemTest, CategoryVolumeResetOnShutdown) {
    audio.setCategoryVolume(SoundCategory::SFX, 0.3f);
    audio.shutdown();
    audio.initHeadless();
    audio.setCommandBufferEnabled(false);
    EXPECT_FLOAT_EQ(audio.getCategoryVolume(SoundCategory::SFX), 1.0f);
}

TEST_F(AudioSystemTest, MultipleCategoryVolumes) {
    audio.setCategoryVolume(SoundCategory::SFX, 0.8f);
    audio.setCategoryVolume(SoundCategory::Music, 0.3f);
    audio.setCategoryVolume(SoundCategory::Ambient, 0.6f);
    audio.setCategoryVolume(SoundCategory::UI, 0.9f);
    EXPECT_FLOAT_EQ(audio.getCategoryVolume(SoundCategory::SFX), 0.8f);
    EXPECT_FLOAT_EQ(audio.getCategoryVolume(SoundCategory::Music), 0.3f);
    EXPECT_FLOAT_EQ(audio.getCategoryVolume(SoundCategory::Ambient), 0.6f);
    EXPECT_FLOAT_EQ(audio.getCategoryVolume(SoundCategory::UI), 0.9f);
}

TEST_F(AudioSystemTest, DefaultPlaySoundUseSFXCategory) {
    Vec3f pos(0.0f, 0.0f, 0.0f);
    // Default overload without category should use SFX
    SoundHandle handle = audio.playSound("nonexistent.wav", pos);
    EXPECT_EQ(handle, InvalidSoundHandle);
    handle = audio.playSoundLooped("nonexistent.wav", pos);
    EXPECT_EQ(handle, InvalidSoundHandle);
}

// --- Reverb Tests ---

TEST_F(AudioSystemTest, ReverbInitializedAfterInit) {
    EXPECT_TRUE(audio.isReverbInitialized());
}

TEST_F(AudioSystemTest, ReverbDefaultParameters) {
    EXPECT_FLOAT_EQ(audio.getReverbDecayTime(), 0.5f);
    EXPECT_FLOAT_EQ(audio.getReverbDamping(), 0.5f);
    EXPECT_FLOAT_EQ(audio.getReverbWetMix(), 0.3f);
}

TEST_F(AudioSystemTest, SetReverbParameters) {
    audio.setReverbParameters(1.5f, 0.7f, 0.6f);
    EXPECT_FLOAT_EQ(audio.getReverbDecayTime(), 1.5f);
    EXPECT_FLOAT_EQ(audio.getReverbDamping(), 0.7f);
    EXPECT_FLOAT_EQ(audio.getReverbWetMix(), 0.6f);
}

TEST_F(AudioSystemTest, SetReverbParametersClampsDecayTime) {
    // Below minimum
    audio.setReverbParameters(0.01f, 0.5f, 0.5f);
    EXPECT_FLOAT_EQ(audio.getReverbDecayTime(), 0.1f);

    // Above maximum
    audio.setReverbParameters(10.0f, 0.5f, 0.5f);
    EXPECT_FLOAT_EQ(audio.getReverbDecayTime(), 3.0f);
}

TEST_F(AudioSystemTest, SetReverbParametersClampsDamping) {
    // Below minimum
    audio.setReverbParameters(0.5f, 0.01f, 0.5f);
    EXPECT_FLOAT_EQ(audio.getReverbDamping(), 0.1f);

    // Above maximum
    audio.setReverbParameters(0.5f, 1.0f, 0.5f);
    EXPECT_FLOAT_EQ(audio.getReverbDamping(), 0.9f);
}

TEST_F(AudioSystemTest, SetReverbParametersClampsWetMix) {
    // Below minimum
    audio.setReverbParameters(0.5f, 0.5f, -0.5f);
    EXPECT_FLOAT_EQ(audio.getReverbWetMix(), 0.0f);

    // Above maximum
    audio.setReverbParameters(0.5f, 0.5f, 2.0f);
    EXPECT_FLOAT_EQ(audio.getReverbWetMix(), 1.0f);
}

TEST_F(AudioSystemTest, MultipleSetReverbParametersLastWins) {
    audio.setReverbParameters(0.3f, 0.2f, 0.1f);
    audio.setReverbParameters(1.0f, 0.5f, 0.4f);
    audio.setReverbParameters(2.5f, 0.8f, 0.9f);
    EXPECT_FLOAT_EQ(audio.getReverbDecayTime(), 2.5f);
    EXPECT_FLOAT_EQ(audio.getReverbDamping(), 0.8f);
    EXPECT_FLOAT_EQ(audio.getReverbWetMix(), 0.9f);
}

TEST_F(AudioSystemTest, ReverbResetOnShutdown) {
    audio.setReverbParameters(2.0f, 0.8f, 0.7f);
    audio.shutdown();
    audio.initHeadless();
    audio.setCommandBufferEnabled(false);
    // After shutdown/reinit, reverb params should be back to defaults
    EXPECT_FLOAT_EQ(audio.getReverbDecayTime(), 0.5f);
    EXPECT_FLOAT_EQ(audio.getReverbDamping(), 0.5f);
    EXPECT_FLOAT_EQ(audio.getReverbWetMix(), 0.3f);
    EXPECT_TRUE(audio.isReverbInitialized());
}

TEST_F(AudioSystemTest, ReverbNotInitializedBeforeInit) {
    AudioSystem uninit;
    EXPECT_FALSE(uninit.isReverbInitialized());
}

TEST_F(AudioSystemTest, SetReverbParametersBeforeInit) {
    AudioSystem uninit;
    // Should not crash, just store params
    uninit.setReverbParameters(1.0f, 0.5f, 0.4f);
    EXPECT_FLOAT_EQ(uninit.getReverbDecayTime(), 1.0f);
    EXPECT_FLOAT_EQ(uninit.getReverbDamping(), 0.5f);
    EXPECT_FLOAT_EQ(uninit.getReverbWetMix(), 0.4f);
}

TEST_F(AudioSystemTest, ReverbInitShutdownCycles) {
    EXPECT_TRUE(audio.isReverbInitialized());
    audio.shutdown();
    EXPECT_FALSE(audio.isReverbInitialized());

    audio.initHeadless();
    audio.setCommandBufferEnabled(false);
    EXPECT_TRUE(audio.isReverbInitialized());

    audio.shutdown();
    EXPECT_FALSE(audio.isReverbInitialized());
}

TEST_F(AudioSystemTest, ReverbWithMapToReverbParams) {
    // Integration: use ReverbZone's mapToReverbParams output
    ZoneEstimate zone;
    zone.volume = 500;
    zone.surfaceArea = 200;
    zone.openness = 0.2f;
    zone.complete = false;

    ReverbParams params = mapToReverbParams(zone);
    audio.setReverbParameters(params.decayTime, params.damping, params.wetMix);

    // Verify clamped values are in valid ranges
    EXPECT_GE(audio.getReverbDecayTime(), 0.1f);
    EXPECT_LE(audio.getReverbDecayTime(), 3.0f);
    EXPECT_GE(audio.getReverbDamping(), 0.1f);
    EXPECT_LE(audio.getReverbDamping(), 0.9f);
    EXPECT_GE(audio.getReverbWetMix(), 0.0f);
    EXPECT_LE(audio.getReverbWetMix(), 1.0f);
}

// --- Reverb Command Buffer Tests ---

TEST_F(AudioCommandBufferTest, SetReverbParamsQueuesCommand) {
    audio.setReverbParameters(1.5f, 0.6f, 0.5f);
    audio.update(0.016f);
    // Should not crash; params stored immediately
    EXPECT_FLOAT_EQ(audio.getReverbDecayTime(), 1.5f);
    EXPECT_FLOAT_EQ(audio.getReverbDamping(), 0.6f);
    EXPECT_FLOAT_EQ(audio.getReverbWetMix(), 0.5f);
}

TEST_F(AudioCommandBufferTest, MultipleReverbParamsQueued) {
    audio.setReverbParameters(0.3f, 0.2f, 0.1f);
    audio.setReverbParameters(2.0f, 0.8f, 0.9f);
    audio.update(0.016f);
    // Last params should win after drain
    EXPECT_FLOAT_EQ(audio.getReverbDecayTime(), 2.0f);
    EXPECT_FLOAT_EQ(audio.getReverbDamping(), 0.8f);
    EXPECT_FLOAT_EQ(audio.getReverbWetMix(), 0.9f);
}

TEST_F(AudioCommandBufferTest, ReverbParamsClampedBeforeQueue) {
    audio.setReverbParameters(-1.0f, 5.0f, -2.0f);
    EXPECT_FLOAT_EQ(audio.getReverbDecayTime(), 0.1f);
    EXPECT_FLOAT_EQ(audio.getReverbDamping(), 0.9f);
    EXPECT_FLOAT_EQ(audio.getReverbWetMix(), 0.0f);
    audio.update(0.016f);
}
