#include "fabric/core/DebrisPool.hh"

#include <gtest/gtest.h>

using namespace fabric;

TEST(DebrisPoolTest, ConstructDefault) {
    DebrisPool pool;
    EXPECT_EQ(pool.activeCount(), 0u);
    EXPECT_EQ(pool.maxActive(), DebrisPool::kDefaultMaxActive);
}

TEST(DebrisPoolTest, ConstructWithMaxActive) {
    DebrisPool pool(100);
    EXPECT_EQ(pool.maxActive(), 100u);
}

TEST(DebrisPoolTest, AddDebrisRequiresUpdateToActivate) {
    DebrisPool pool;
    pool.add(0, 10, 0, 1.0f);
    EXPECT_EQ(pool.activeCount(), 0u);

    pool.update(0.016f);
    EXPECT_EQ(pool.activeCount(), 1u);
}

TEST(DebrisPoolTest, MaxActiveLimitAppliedFromPendingQueue) {
    DebrisPool pool(3);
    for (int i = 0; i < 10; ++i) {
        pool.add(i * 2, 10, 0, 1.0f);
    }

    pool.update(0.016f);
    EXPECT_EQ(pool.activeCount(), 3u);
}

TEST(DebrisPoolTest, GravityAppliedToActiveDebris) {
    DebrisPool pool;
    pool.add(0, 10, 0, 1.0f);
    pool.update(0.016f);

    const auto& debris = pool.getDebris();
    ASSERT_EQ(debris.size(), 1u);
    EXPECT_FLOAT_EQ(debris[0].velocity.y, -9.81f * 0.016f);
}

TEST(DebrisPoolTest, GroundCollisionClampsToGround) {
    DebrisPool pool;
    pool.add(0, 0, 0, 1.0f);

    pool.update(1.0f);

    const auto& debris = pool.getDebris();
    ASSERT_EQ(debris.size(), 1u);
    EXPECT_GE(debris[0].position.y, 0.0f);
}

TEST(DebrisPoolTest, SetMaxActiveTrimsExistingDebris) {
    DebrisPool pool(10);
    pool.setMergeDistance(-1.0f);
    for (int i = 0; i < 10; ++i) {
        pool.add(i * 3, 10, 0, 1.0f);
    }

    pool.update(0.016f);
    ASSERT_EQ(pool.activeCount(), 10u);

    pool.setMaxActive(5);
    EXPECT_EQ(pool.activeCount(), 5u);
    EXPECT_EQ(pool.maxActive(), 5u);
}

TEST(DebrisPoolTest, MergeNearbyCombinesDebrisWhenWithinDistance) {
    DebrisPool pool;
    using Vec3 = Vector3<float, Space::World>;

    pool.add(Vec3(0, 5, 0), 1.0f, 0.5f);
    pool.add(Vec3(0.5f, 5, 0), 1.0f, 0.5f);
    pool.setMergeDistance(0.2f);

    pool.update(0.016f);

    const auto& debris = pool.getDebris();
    ASSERT_EQ(debris.size(), 1u);
    EXPECT_FLOAT_EQ(debris[0].density, 2.0f);
}

TEST(DebrisPoolTest, ClearRemovesActiveAndPending) {
    DebrisPool pool;
    for (int i = 0; i < 5; ++i) {
        pool.add(i * 2, 10, 0, 1.0f);
    }

    pool.update(0.016f);
    ASSERT_EQ(pool.activeCount(), 5u);

    pool.clear();
    EXPECT_EQ(pool.activeCount(), 0u);
}

TEST(DebrisPoolTest, LifetimeExpiryRemovesDebris) {
    DebrisPool pool;
    using Vec3 = Vector3<float, Space::World>;

    pool.add(Vec3(0, 5, 0), 1.0f, 0.5f);
    pool.update(0.0f);
    ASSERT_EQ(pool.activeCount(), 1u);

    for (int i = 0; i < 110; ++i) {
        pool.update(0.1f);
    }

    EXPECT_EQ(pool.activeCount(), 0u);
}

TEST(DebrisPoolTest, ParticleConversionEmitsAndRemovesDebris) {
    DebrisPool pool;
    using Vec3 = Vector3<float, Space::World>;

    bool emitted = false;
    int emittedCount = 0;
    Vec3 emittedPos;

    pool.setParticleEmitter([&](const Vec3& pos, float, int count) {
        emitted = true;
        emittedCount = count;
        emittedPos = pos;
    });
    pool.enableParticleConversion(true);
    pool.setParticleConvertLifetime(2.0f);

    pool.add(Vec3(0, 5, 0), 1.0f, 0.5f);
    pool.update(0.0f);

    for (int i = 0; i < 11; ++i) {
        pool.update(0.1f);
    }

    EXPECT_TRUE(emitted);
    EXPECT_GT(emittedCount, 0);
    EXPECT_FLOAT_EQ(emittedPos.x, 0.0f);
    EXPECT_EQ(pool.activeCount(), 0u);
}

TEST(DebrisPoolTest, ParticleConversionWithoutEmitterLeavesDebrisUntouchedUntilLifetimeExpiry) {
    DebrisPool pool;
    pool.enableParticleConversion(true);
    pool.setParticleConvertLifetime(0.5f);
    pool.add(0, 5, 0, 1.0f);
    pool.update(0.0f);

    for (int i = 0; i < 4; ++i) {
        pool.update(0.1f);
    }

    EXPECT_EQ(pool.activeCount(), 1u);
}

TEST(DebrisPoolTest, SleepStateCanBeReachedWithThresholdAndFrames) {
    DebrisPool pool;
    pool.add(0, 0, 0, 1.0f);
    pool.setSleepThreshold(1.0f);
    pool.setSleepFrames(2);

    pool.update(0.0f);
    pool.update(0.0f);

    const auto& debris = pool.getDebris();
    ASSERT_EQ(debris.size(), 1u);
    EXPECT_TRUE(debris[0].sleeping);
}

TEST(DebrisPoolTest, GetDebrisReturnsAddedPositionDensity) {
    DebrisPool pool;
    pool.add(5, 10, 7, 1.0f);
    pool.update(0.0f);

    const auto& debris = pool.getDebris();
    ASSERT_EQ(debris.size(), 1u);
    EXPECT_FLOAT_EQ(debris[0].position.x, 5.0f);
    EXPECT_FLOAT_EQ(debris[0].position.y, 10.0f);
    EXPECT_FLOAT_EQ(debris[0].position.z, 7.0f);
    EXPECT_FLOAT_EQ(debris[0].density, 1.0f);
}

TEST(DebrisPoolTest, PendingQueueDrainsWhenCapacityRaised) {
    DebrisPool pool(2);
    for (int i = 0; i < 10; ++i) {
        pool.add(i * 2, 10, 0, 1.0f);
    }

    pool.update(0.016f);
    EXPECT_EQ(pool.activeCount(), 2u);

    pool.setMergeDistance(-1.0f);
    pool.setMaxActive(10);
    pool.update(0.016f);

    EXPECT_EQ(pool.activeCount(), 10u);
}
