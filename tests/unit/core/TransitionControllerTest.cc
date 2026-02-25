#include <gtest/gtest.h>
#include "fabric/core/TransitionController.hh"
#include <cmath>

using namespace fabric;

class TransitionControllerTest : public ::testing::Test {
  protected:
    TransitionController tc;
    ChunkedGrid<float> grid;

    void fillFloor(int y, int xMin, int xMax, int zMin, int zMax) {
        for (int z = zMin; z <= zMax; ++z)
            for (int x = xMin; x <= xMax; ++x)
                grid.set(x, y, z, 1.0f);
    }
};

// enterFlight tests

TEST_F(TransitionControllerTest, EnterFlightPreservesMomentumScaled) {
    Vec3f vel(10.0f, 0.0f, 5.0f);
    auto result = tc.enterFlight(vel, 5.0f, 0.8f);

    EXPECT_NEAR(result.velocity.x, 8.0f, 0.01f);
    EXPECT_NEAR(result.velocity.z, 4.0f, 0.01f);
}

TEST_F(TransitionControllerTest, EnterFlightAddsUpwardImpulse) {
    Vec3f vel(0.0f, 0.0f, 0.0f);
    auto result = tc.enterFlight(vel, 7.5f);

    EXPECT_NEAR(result.velocity.y, 7.5f, 0.01f);
}

TEST_F(TransitionControllerTest, EnterFlightReturnsFlyingState) {
    Vec3f vel(3.0f, 1.0f, 2.0f);
    auto result = tc.enterFlight(vel);

    EXPECT_EQ(result.newState, CharacterState::Flying);
}

TEST_F(TransitionControllerTest, EnterFlightDefaultParams) {
    Vec3f vel(10.0f, 0.0f, 10.0f);
    auto result = tc.enterFlight(vel);

    // Default: impulse=5.0, scale=0.8
    EXPECT_NEAR(result.velocity.x, 8.0f, 0.01f);
    EXPECT_NEAR(result.velocity.y, 5.0f, 0.01f);
    EXPECT_NEAR(result.velocity.z, 8.0f, 0.01f);
}

TEST_F(TransitionControllerTest, EnterFlightZeroVelocity) {
    Vec3f vel(0.0f, 0.0f, 0.0f);
    auto result = tc.enterFlight(vel, 5.0f, 0.8f);

    EXPECT_NEAR(result.velocity.x, 0.0f, 0.01f);
    EXPECT_NEAR(result.velocity.y, 5.0f, 0.01f);
    EXPECT_NEAR(result.velocity.z, 0.0f, 0.01f);
}

// exitFlight tests

TEST_F(TransitionControllerTest, ExitFlightNearGroundGrounded) {
    fillFloor(0, -5, 5, -5, 5);

    Vec3f pos(0.0f, 1.5f, 0.0f);
    Vec3f vel(5.0f, -2.0f, 3.0f);
    auto result = tc.exitFlight(vel, pos, grid, 2.0f);

    EXPECT_EQ(result.newState, CharacterState::Grounded);
    EXPECT_NEAR(result.velocity.y, 0.0f, 0.01f);
}

TEST_F(TransitionControllerTest, ExitFlightPreservesHorizontalOnLand) {
    fillFloor(0, -5, 5, -5, 5);

    Vec3f pos(0.0f, 1.5f, 0.0f);
    Vec3f vel(10.0f, -3.0f, 7.0f);
    auto result = tc.exitFlight(vel, pos, grid, 2.0f);

    EXPECT_NEAR(result.velocity.x, 10.0f, 0.01f);
    EXPECT_NEAR(result.velocity.z, 7.0f, 0.01f);
}

TEST_F(TransitionControllerTest, ExitFlightInAirFalling) {
    // No ground anywhere
    Vec3f pos(0.0f, 50.0f, 0.0f);
    Vec3f vel(5.0f, 0.0f, 0.0f);
    auto result = tc.exitFlight(vel, pos, grid, 2.0f);

    EXPECT_EQ(result.newState, CharacterState::Falling);
}

TEST_F(TransitionControllerTest, ExitFlightFallingPreservesVelocity) {
    Vec3f pos(0.0f, 50.0f, 0.0f);
    Vec3f vel(5.0f, -3.0f, 2.0f);
    auto result = tc.exitFlight(vel, pos, grid, 2.0f);

    EXPECT_NEAR(result.velocity.x, 5.0f, 0.01f);
    EXPECT_NEAR(result.velocity.y, -3.0f, 0.01f);
    EXPECT_NEAR(result.velocity.z, 2.0f, 0.01f);
}

TEST_F(TransitionControllerTest, ExitFlightGroundJustBeyondRange) {
    fillFloor(0, -5, 5, -5, 5);

    // Position at y=3.5, groundCheckDistance=2.0: scans y=2 down to y=1, misses floor at y=0
    Vec3f pos(0.0f, 3.5f, 0.0f);
    Vec3f vel(0.0f, 0.0f, 0.0f);
    auto result = tc.exitFlight(vel, pos, grid, 2.0f);

    EXPECT_EQ(result.newState, CharacterState::Falling);
}

TEST_F(TransitionControllerTest, ExitFlightLargerCheckDistance) {
    fillFloor(0, -5, 5, -5, 5);

    // Same position but larger check distance finds the floor
    Vec3f pos(0.0f, 3.5f, 0.0f);
    Vec3f vel(0.0f, 0.0f, 0.0f);
    auto result = tc.exitFlight(vel, pos, grid, 5.0f);

    EXPECT_EQ(result.newState, CharacterState::Grounded);
}
