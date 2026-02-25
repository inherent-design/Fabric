#include "fabric/core/MeleeSystem.hh"
#include <gtest/gtest.h>

using namespace fabric;

class MeleeSystemTest : public ::testing::Test {
  protected:
    EventDispatcher dispatcher;
};

TEST_F(MeleeSystemTest, AttackHitboxPositionedInFrontPosZ) {
    MeleeSystem ms(dispatcher);
    MeleeConfig config;
    config.reach = 4.0f;
    config.width = 2.0f;
    config.height = 2.0f;

    Vec3f pos(0.0f, 0.0f, 0.0f);
    Vec3f facing(0.0f, 0.0f, 1.0f);
    auto attack = ms.createAttack(pos, facing, config);

    // Center should be at (0, 0, 2) = pos + facing * reach/2
    auto center = attack.hitbox.center();
    EXPECT_NEAR(center.x, 0.0f, 0.01f);
    EXPECT_NEAR(center.y, 0.0f, 0.01f);
    EXPECT_NEAR(center.z, 2.0f, 0.01f);

    // Extents: (width/2, height/2, reach/2) = (1, 1, 2)
    EXPECT_NEAR(attack.hitbox.min.x, -1.0f, 0.01f);
    EXPECT_NEAR(attack.hitbox.max.x, 1.0f, 0.01f);
    EXPECT_NEAR(attack.hitbox.min.z, 0.0f, 0.01f);
    EXPECT_NEAR(attack.hitbox.max.z, 4.0f, 0.01f);
}

TEST_F(MeleeSystemTest, AttackHitboxFacingNegX) {
    MeleeSystem ms(dispatcher);
    MeleeConfig config;
    config.reach = 3.0f;

    Vec3f pos(10.0f, 5.0f, 10.0f);
    Vec3f facing(-1.0f, 0.0f, 0.0f);
    auto attack = ms.createAttack(pos, facing, config);

    auto center = attack.hitbox.center();
    EXPECT_NEAR(center.x, 8.5f, 0.01f); // 10 - 1.5
    EXPECT_NEAR(center.y, 5.0f, 0.01f);
    EXPECT_NEAR(center.z, 10.0f, 0.01f);
}

TEST_F(MeleeSystemTest, AttackHitboxFacingPosY) {
    MeleeSystem ms(dispatcher);
    MeleeConfig config;
    config.reach = 3.0f;
    config.width = 2.0f;

    Vec3f pos(0.0f, 0.0f, 0.0f);
    Vec3f facing(0.0f, 1.0f, 0.0f);
    auto attack = ms.createAttack(pos, facing, config);

    auto center = attack.hitbox.center();
    EXPECT_NEAR(center.y, 1.5f, 0.01f);
    // Y-dominant: halfExtents = (width/2, reach/2, width/2)
    EXPECT_NEAR(attack.hitbox.min.x, -1.0f, 0.01f);
    EXPECT_NEAR(attack.hitbox.max.x, 1.0f, 0.01f);
}

TEST_F(MeleeSystemTest, HitDetectionTargetInside) {
    MeleeSystem ms(dispatcher);
    MeleeConfig config;
    config.reach = 4.0f;
    config.width = 2.0f;
    config.height = 2.0f;

    auto attack = ms.createAttack(Vec3f(0, 0, 0), Vec3f(0, 0, 1), config);

    std::vector<AABB> targets = {
        AABB(Vec3f(-0.5f, -0.5f, 1.5f), Vec3f(0.5f, 0.5f, 2.5f))
    };

    auto hits = ms.checkHits(attack, targets);
    ASSERT_EQ(hits.size(), 1u);
    EXPECT_EQ(hits[0], 0u);
}

TEST_F(MeleeSystemTest, MissDetectionTargetOutside) {
    MeleeSystem ms(dispatcher);
    MeleeConfig config;
    config.reach = 4.0f;
    config.width = 2.0f;
    config.height = 2.0f;

    auto attack = ms.createAttack(Vec3f(0, 0, 0), Vec3f(0, 0, 1), config);

    std::vector<AABB> targets = {
        AABB(Vec3f(10.0f, 10.0f, 10.0f), Vec3f(11.0f, 11.0f, 11.0f))
    };

    auto hits = ms.checkHits(attack, targets);
    EXPECT_TRUE(hits.empty());
}

TEST_F(MeleeSystemTest, MultipleTargetsHitsAll) {
    MeleeSystem ms(dispatcher);
    MeleeConfig config;
    config.reach = 4.0f;
    config.width = 2.0f;
    config.height = 2.0f;

    auto attack = ms.createAttack(Vec3f(0, 0, 0), Vec3f(0, 0, 1), config);

    std::vector<AABB> targets = {
        AABB(Vec3f(-0.5f, -0.5f, 1.0f), Vec3f(0.5f, 0.5f, 2.0f)),
        AABB(Vec3f(-0.5f, -0.5f, 3.0f), Vec3f(0.5f, 0.5f, 3.5f)),
        AABB(Vec3f(20.0f, 20.0f, 20.0f), Vec3f(21.0f, 21.0f, 21.0f))
    };

    auto hits = ms.checkHits(attack, targets);
    ASSERT_EQ(hits.size(), 2u);
    EXPECT_EQ(hits[0], 0u);
    EXPECT_EQ(hits[1], 1u);
}

TEST_F(MeleeSystemTest, CooldownBlocksAttack) {
    MeleeSystem ms(dispatcher);
    EXPECT_FALSE(ms.canAttack(0.5f));
    EXPECT_TRUE(ms.canAttack(0.0f));
    EXPECT_TRUE(ms.canAttack(-0.1f));
}

TEST_F(MeleeSystemTest, CooldownUpdateDecrementsCorrectly) {
    MeleeSystem ms(dispatcher);
    float remaining = ms.updateCooldown(0.5f, 0.2f);
    EXPECT_NEAR(remaining, 0.3f, 0.001f);
}

TEST_F(MeleeSystemTest, CooldownUpdateClampsToZero) {
    MeleeSystem ms(dispatcher);
    float remaining = ms.updateCooldown(0.1f, 0.5f);
    EXPECT_FLOAT_EQ(remaining, 0.0f);
}

TEST_F(MeleeSystemTest, DamageEventDispatched) {
    MeleeSystem ms(dispatcher);
    float receivedDamage = 0.0f;
    dispatcher.addEventListener("melee_damage", [&](Event& e) {
        receivedDamage = e.getData<float>("damage");
    });

    ms.emitDamageEvent(Vec3f(1, 2, 3), 25.0f, Vec3f(0, 0, 1));
    EXPECT_FLOAT_EQ(receivedDamage, 25.0f);
}

TEST_F(MeleeSystemTest, AttackDirectionNormalized) {
    MeleeSystem ms(dispatcher);
    MeleeConfig config;
    Vec3f facing(3.0f, 0.0f, 4.0f); // length = 5
    auto attack = ms.createAttack(Vec3f(0, 0, 0), facing, config);

    float len = std::sqrt(
        attack.direction.x * attack.direction.x +
        attack.direction.y * attack.direction.y +
        attack.direction.z * attack.direction.z);
    EXPECT_NEAR(len, 1.0f, 0.001f);
    EXPECT_NEAR(attack.direction.x, 0.6f, 0.001f);
    EXPECT_NEAR(attack.direction.z, 0.8f, 0.001f);
}

TEST_F(MeleeSystemTest, AttackCarriesConfigValues) {
    MeleeSystem ms(dispatcher);
    MeleeConfig config;
    config.damage = 42.0f;
    config.knockback = 7.5f;

    auto attack = ms.createAttack(Vec3f(0, 0, 0), Vec3f(0, 0, 1), config);
    EXPECT_FLOAT_EQ(attack.damage, 42.0f);
    EXPECT_FLOAT_EQ(attack.knockback, 7.5f);
}
