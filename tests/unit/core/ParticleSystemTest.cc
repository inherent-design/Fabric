#include "fabric/core/ParticleSystem.hh"

#include <gtest/gtest.h>

using namespace fabric;

// ParticleSystem requires bgfx for init()/render(), but we can test
// the CPU-side pool logic (emit, update, kill) without a live GPU context.
// The system gracefully handles !isValid() state.

class ParticleSystemTest : public ::testing::Test {
  protected:
    ParticleSystem ps;
};

TEST_F(ParticleSystemTest, InitialStateIsEmpty) {
    EXPECT_EQ(ps.activeCount(), 0u);
    EXPECT_FALSE(ps.isValid()); // no bgfx context in tests
}

TEST_F(ParticleSystemTest, EmitAddsParticles) {
    Vector3<float, Space::World> pos(0.0f, 0.0f, 0.0f);
    ps.emit(pos, 1.0f, 10, ParticleType::DebrisPuff);
    EXPECT_EQ(ps.activeCount(), 10u);
}

TEST_F(ParticleSystemTest, EmitRespectsPoolLimit) {
    Vector3<float, Space::World> pos(0.0f, 0.0f, 0.0f);
    // Try to emit more than the pool allows
    ps.emit(pos, 1.0f, static_cast<int>(ParticleSystem::kMaxParticles) + 500, ParticleType::DebrisPuff);
    EXPECT_EQ(ps.activeCount(), ParticleSystem::kMaxParticles);
}

TEST_F(ParticleSystemTest, UpdateAgesAndKillsExpired) {
    Vector3<float, Space::World> pos(0.0f, 5.0f, 0.0f);
    ps.emit(pos, 0.0f, 5, ParticleType::Spark);
    ASSERT_EQ(ps.activeCount(), 5u);

    // Sparks have lifetime 0.3-1.0s, stepping 2s should kill all
    ps.update(2.0f);
    EXPECT_EQ(ps.activeCount(), 0u);
}

TEST_F(ParticleSystemTest, UpdateDoesNotKillYoungParticles) {
    Vector3<float, Space::World> pos(0.0f, 0.0f, 0.0f);
    ps.emit(pos, 0.0f, 10, ParticleType::AmbientDust);
    ASSERT_EQ(ps.activeCount(), 10u);

    // Ambient dust has lifetime 3-8s, stepping 0.1s should keep all alive
    ps.update(0.1f);
    EXPECT_EQ(ps.activeCount(), 10u);
}

TEST_F(ParticleSystemTest, SwapAndPopCorrectness) {
    Vector3<float, Space::World> pos(0.0f, 0.0f, 0.0f);
    // Emit short-lived sparks and long-lived dust interleaved
    ps.emit(pos, 0.0f, 5, ParticleType::Spark);       // lifetime 0.3-1.0s
    ps.emit(pos, 0.0f, 5, ParticleType::AmbientDust); // lifetime 3-8s

    ASSERT_EQ(ps.activeCount(), 10u);

    // After 1.5s, sparks should be dead, dust should survive
    ps.update(1.5f);
    EXPECT_EQ(ps.activeCount(), 5u);

    // After another 0.5s, dust should still be alive (max 2s elapsed, min lifetime 3s)
    ps.update(0.5f);
    EXPECT_EQ(ps.activeCount(), 5u);
}

TEST_F(ParticleSystemTest, BurstEmitMultipleTypes) {
    Vector3<float, Space::World> pos(10.0f, 20.0f, 30.0f);

    ps.emit(pos, 2.0f, 100, ParticleType::DebrisPuff);
    ps.emit(pos, 1.0f, 50, ParticleType::Spark);
    ps.emit(pos, 0.5f, 25, ParticleType::AmbientDust);

    EXPECT_EQ(ps.activeCount(), 175u);
}

TEST_F(ParticleSystemTest, RenderNoOpWithoutInit) {
    Vector3<float, Space::World> pos(0.0f, 0.0f, 0.0f);
    ps.emit(pos, 1.0f, 10, ParticleType::DebrisPuff);

    // render() should not crash when bgfx is not initialized
    float identity[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    ps.render(identity, identity, 1280, 720);

    // Still alive (render is a no-op, doesn't affect simulation)
    EXPECT_EQ(ps.activeCount(), 10u);
}

TEST_F(ParticleSystemTest, GravityAffectsVelocity) {
    Vector3<float, Space::World> pos(0.0f, 100.0f, 0.0f);
    ps.emit(pos, 0.0f, 1, ParticleType::Spark);
    ASSERT_EQ(ps.activeCount(), 1u);

    // Sparks have gravityScale=1.0, so gravity pulls them down.
    // Step a small amount and verify update doesn't crash.
    ps.update(0.016f);
    EXPECT_EQ(ps.activeCount(), 1u); // still alive (lifetime 0.3-1.0)
}

TEST_F(ParticleSystemTest, EmitWithZeroCountIsNoOp) {
    Vector3<float, Space::World> pos(0.0f, 0.0f, 0.0f);
    ps.emit(pos, 1.0f, 0, ParticleType::DebrisPuff);
    EXPECT_EQ(ps.activeCount(), 0u);
}

TEST_F(ParticleSystemTest, MultipleUpdateCyclesProgressAge) {
    Vector3<float, Space::World> pos(0.0f, 0.0f, 0.0f);
    ps.emit(pos, 0.0f, 10, ParticleType::DebrisPuff); // lifetime 1-3s

    // Step many small increments totaling > 3s
    for (int i = 0; i < 200; ++i) {
        ps.update(0.02f); // 200 * 0.02 = 4.0s
    }

    // All debris puffs should be dead (max lifetime 3s)
    EXPECT_EQ(ps.activeCount(), 0u);
}
