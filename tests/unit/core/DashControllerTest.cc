#include "fabric/core/DashController.hh"
#include <cmath>
#include <gtest/gtest.h>

using namespace fabric;

class DashControllerTest : public ::testing::Test {
  protected:
    DashController dc;
    DashState state;
    CharacterConfig config;
};

// startDash tests

TEST_F(DashControllerTest, StartDashSucceedsOffCooldown) {
    bool ok = dc.startDash(state, config, false);

    EXPECT_TRUE(ok);
    EXPECT_TRUE(state.active);
    EXPECT_NEAR(state.durationRemaining, config.dashDuration, 0.001f);
}

TEST_F(DashControllerTest, StartDashSetsCooldown) {
    dc.startDash(state, config, false);

    EXPECT_NEAR(state.cooldownRemaining, config.dashCooldown, 0.001f);
}

TEST_F(DashControllerTest, StartDashFailsDuringCooldown) {
    state.cooldownRemaining = 0.5f;

    bool ok = dc.startDash(state, config, false);
    EXPECT_FALSE(ok);
    EXPECT_FALSE(state.active);
}

TEST_F(DashControllerTest, StartBoostUsesBoostCooldown) {
    dc.startDash(state, config, true);

    EXPECT_NEAR(state.cooldownRemaining, config.boostCooldown, 0.001f);
}

// update tests

TEST_F(DashControllerTest, UpdateReturnsDisplacement) {
    dc.startDash(state, config, false);

    Vec3f dir(1.0f, 0.0f, 0.0f);
    float dt = 1.0f / 60.0f;
    auto result = dc.update(state, config, dir, dt, false);

    EXPECT_TRUE(result.active);
    EXPECT_NEAR(result.displacement.x, config.dashSpeed * dt, 0.01f);
    EXPECT_NEAR(result.displacement.y, 0.0f, 0.001f);
    EXPECT_NEAR(result.displacement.z, 0.0f, 0.001f);
}

TEST_F(DashControllerTest, BoostUsesBoostSpeed) {
    dc.startDash(state, config, true);

    Vec3f dir(0.0f, 1.0f, 0.0f);
    float dt = 1.0f / 60.0f;
    auto result = dc.update(state, config, dir, dt, true);

    EXPECT_NEAR(result.displacement.y, config.boostSpeed * dt, 0.01f);
}

TEST_F(DashControllerTest, UpdateInactiveReturnsZero) {
    Vec3f dir(1.0f, 0.0f, 0.0f);
    auto result = dc.update(state, config, dir, 1.0f / 60.0f, false);

    EXPECT_FALSE(result.active);
    EXPECT_NEAR(result.displacement.x, 0.0f, 0.001f);
}

TEST_F(DashControllerTest, DashAutoEndsAfterDuration) {
    dc.startDash(state, config, false);

    Vec3f dir(1.0f, 0.0f, 0.0f);
    // Tick past entire duration in one step
    auto result = dc.update(state, config, dir, config.dashDuration + 0.01f, false);

    EXPECT_TRUE(result.justFinished);
    EXPECT_FALSE(result.active);
    EXPECT_FALSE(state.active);
}

TEST_F(DashControllerTest, DashNotFinishedMidDuration) {
    dc.startDash(state, config, false);

    Vec3f dir(1.0f, 0.0f, 0.0f);
    auto result = dc.update(state, config, dir, config.dashDuration * 0.5f, false);

    EXPECT_TRUE(result.active);
    EXPECT_FALSE(result.justFinished);
    EXPECT_TRUE(state.active);
}

// updateCooldown tests

TEST_F(DashControllerTest, CooldownDecrementsOverTime) {
    dc.startDash(state, config, false);
    float initial = state.cooldownRemaining;

    dc.updateCooldown(state, 0.1f);
    EXPECT_NEAR(state.cooldownRemaining, initial - 0.1f, 0.001f);
}

TEST_F(DashControllerTest, CooldownClampsToZero) {
    state.cooldownRemaining = 0.05f;

    dc.updateCooldown(state, 1.0f);
    EXPECT_NEAR(state.cooldownRemaining, 0.0f, 0.001f);
}

TEST_F(DashControllerTest, CooldownExpiresAllowsNewDash) {
    dc.startDash(state, config, false);

    // Expire the dash
    dc.update(state, config, Vec3f(1.0f, 0.0f, 0.0f), config.dashDuration + 0.01f, false);

    // Expire cooldown
    dc.updateCooldown(state, config.dashCooldown + 0.01f);
    EXPECT_NEAR(state.cooldownRemaining, 0.0f, 0.02f);

    bool ok = dc.startDash(state, config, false);
    EXPECT_TRUE(ok);
}

TEST_F(DashControllerTest, DiagonalDashDisplacement) {
    dc.startDash(state, config, false);

    // Diagonal direction (not normalized -- caller is responsible)
    Vec3f dir(0.707f, 0.0f, 0.707f);
    float dt = 1.0f / 60.0f;
    auto result = dc.update(state, config, dir, dt, false);

    float expectedX = 0.707f * config.dashSpeed * dt;
    float expectedZ = 0.707f * config.dashSpeed * dt;
    EXPECT_NEAR(result.displacement.x, expectedX, 0.01f);
    EXPECT_NEAR(result.displacement.z, expectedZ, 0.01f);
}
