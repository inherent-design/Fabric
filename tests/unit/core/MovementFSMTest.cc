#include "recurse/character/MovementFSM.hh"
#include <gtest/gtest.h>

using namespace fabric;
using namespace recurse;

class MovementFSMTest : public ::testing::Test {
  protected:
    MovementFSM fsm;
};

TEST_F(MovementFSMTest, DefaultStateIsGrounded) {
    EXPECT_EQ(fsm.currentState(), CharacterState::Grounded);
}

TEST_F(MovementFSMTest, GroundedToJumping) {
    EXPECT_TRUE(fsm.tryTransition(CharacterState::Jumping));
    EXPECT_EQ(fsm.currentState(), CharacterState::Jumping);
}

TEST_F(MovementFSMTest, JumpingToFalling) {
    fsm.tryTransition(CharacterState::Jumping);
    EXPECT_TRUE(fsm.tryTransition(CharacterState::Falling));
    EXPECT_EQ(fsm.currentState(), CharacterState::Falling);
}

TEST_F(MovementFSMTest, FallingToGrounded) {
    fsm.tryTransition(CharacterState::Jumping);
    fsm.tryTransition(CharacterState::Falling);
    EXPECT_TRUE(fsm.tryTransition(CharacterState::Grounded));
    EXPECT_EQ(fsm.currentState(), CharacterState::Grounded);
}

TEST_F(MovementFSMTest, GroundedToFlying) {
    EXPECT_TRUE(fsm.tryTransition(CharacterState::Flying));
    EXPECT_EQ(fsm.currentState(), CharacterState::Flying);
}

TEST_F(MovementFSMTest, FlyingToFalling) {
    fsm.tryTransition(CharacterState::Flying);
    EXPECT_TRUE(fsm.tryTransition(CharacterState::Falling));
    EXPECT_EQ(fsm.currentState(), CharacterState::Falling);
}

TEST_F(MovementFSMTest, GroundedToDashing) {
    EXPECT_TRUE(fsm.tryTransition(CharacterState::Dashing));
    EXPECT_EQ(fsm.currentState(), CharacterState::Dashing);
}

TEST_F(MovementFSMTest, FlyingToBoosting) {
    fsm.tryTransition(CharacterState::Flying);
    EXPECT_TRUE(fsm.tryTransition(CharacterState::Boosting));
    EXPECT_EQ(fsm.currentState(), CharacterState::Boosting);
}

TEST_F(MovementFSMTest, InvalidTransitionGroundedToRagdoll) {
    EXPECT_FALSE(fsm.tryTransition(CharacterState::Ragdoll));
    EXPECT_EQ(fsm.currentState(), CharacterState::Grounded);
}

TEST_F(MovementFSMTest, InvalidTransitionDashingToSwimming) {
    fsm.tryTransition(CharacterState::Dashing);
    EXPECT_FALSE(fsm.tryTransition(CharacterState::Swimming));
    EXPECT_EQ(fsm.currentState(), CharacterState::Dashing);
}

TEST_F(MovementFSMTest, StateQueriesGrounded) {
    EXPECT_TRUE(fsm.isGrounded());
    EXPECT_FALSE(fsm.isAirborne());
    EXPECT_FALSE(fsm.isFlying());
    EXPECT_TRUE(fsm.canDash());
}

TEST_F(MovementFSMTest, StateQueriesJumping) {
    fsm.tryTransition(CharacterState::Jumping);
    EXPECT_FALSE(fsm.isGrounded());
    EXPECT_TRUE(fsm.isAirborne());
    EXPECT_FALSE(fsm.isFlying());
    EXPECT_FALSE(fsm.canDash());
}

TEST_F(MovementFSMTest, StateQueriesFalling) {
    fsm.tryTransition(CharacterState::Falling);
    EXPECT_FALSE(fsm.isGrounded());
    EXPECT_TRUE(fsm.isAirborne());
    EXPECT_FALSE(fsm.isFlying());
    EXPECT_FALSE(fsm.canDash());
}

TEST_F(MovementFSMTest, StateQueriesFlying) {
    fsm.tryTransition(CharacterState::Flying);
    EXPECT_FALSE(fsm.isGrounded());
    EXPECT_FALSE(fsm.isAirborne());
    EXPECT_TRUE(fsm.isFlying());
    EXPECT_FALSE(fsm.canDash());
}

TEST_F(MovementFSMTest, StateQueriesBoosting) {
    fsm.tryTransition(CharacterState::Flying);
    fsm.tryTransition(CharacterState::Boosting);
    EXPECT_FALSE(fsm.isGrounded());
    EXPECT_FALSE(fsm.isAirborne());
    EXPECT_TRUE(fsm.isFlying());
    EXPECT_FALSE(fsm.canDash());
}

TEST_F(MovementFSMTest, FullJumpCycle) {
    EXPECT_TRUE(fsm.tryTransition(CharacterState::Jumping));
    EXPECT_TRUE(fsm.tryTransition(CharacterState::Falling));
    EXPECT_TRUE(fsm.tryTransition(CharacterState::Grounded));
    EXPECT_TRUE(fsm.isGrounded());
}

TEST_F(MovementFSMTest, BoostingReturnToFlying) {
    fsm.tryTransition(CharacterState::Flying);
    fsm.tryTransition(CharacterState::Boosting);
    EXPECT_TRUE(fsm.tryTransition(CharacterState::Flying));
    EXPECT_EQ(fsm.currentState(), CharacterState::Flying);
}

TEST_F(MovementFSMTest, SelfTransitionIsNoop) {
    EXPECT_TRUE(fsm.tryTransition(CharacterState::Grounded));
    EXPECT_EQ(fsm.currentState(), CharacterState::Grounded);
}

// -- Tests for 6 unwired states: Climbing, Swimming, WallRunning, Hanging, Sliding, Ragdoll --
// These states are declared in CharacterTypes.hh but have no addTransition() calls yet.
// Tests verify that no active state can transition to them (future-sprint placeholders).

TEST_F(MovementFSMTest, ClimbingUnreachableFromGrounded) {
    EXPECT_FALSE(fsm.tryTransition(CharacterState::Climbing));
    EXPECT_EQ(fsm.currentState(), CharacterState::Grounded);
}

TEST_F(MovementFSMTest, ClimbingUnreachableFromFalling) {
    fsm.tryTransition(CharacterState::Jumping);
    fsm.tryTransition(CharacterState::Falling);
    EXPECT_FALSE(fsm.tryTransition(CharacterState::Climbing));
    EXPECT_EQ(fsm.currentState(), CharacterState::Falling);
}

TEST_F(MovementFSMTest, ClimbingUnreachableFromFlying) {
    fsm.tryTransition(CharacterState::Flying);
    EXPECT_FALSE(fsm.tryTransition(CharacterState::Climbing));
    EXPECT_EQ(fsm.currentState(), CharacterState::Flying);
}

TEST_F(MovementFSMTest, SwimmingUnreachableFromGrounded) {
    EXPECT_FALSE(fsm.tryTransition(CharacterState::Swimming));
    EXPECT_EQ(fsm.currentState(), CharacterState::Grounded);
}

TEST_F(MovementFSMTest, SwimmingUnreachableFromFalling) {
    fsm.tryTransition(CharacterState::Jumping);
    fsm.tryTransition(CharacterState::Falling);
    EXPECT_FALSE(fsm.tryTransition(CharacterState::Swimming));
    EXPECT_EQ(fsm.currentState(), CharacterState::Falling);
}

TEST_F(MovementFSMTest, SwimmingUnreachableFromFlying) {
    fsm.tryTransition(CharacterState::Flying);
    EXPECT_FALSE(fsm.tryTransition(CharacterState::Swimming));
    EXPECT_EQ(fsm.currentState(), CharacterState::Flying);
}

TEST_F(MovementFSMTest, WallRunningUnreachableFromGrounded) {
    EXPECT_FALSE(fsm.tryTransition(CharacterState::WallRunning));
    EXPECT_EQ(fsm.currentState(), CharacterState::Grounded);
}

TEST_F(MovementFSMTest, WallRunningUnreachableFromJumping) {
    fsm.tryTransition(CharacterState::Jumping);
    EXPECT_FALSE(fsm.tryTransition(CharacterState::WallRunning));
    EXPECT_EQ(fsm.currentState(), CharacterState::Jumping);
}

TEST_F(MovementFSMTest, HangingUnreachableFromGrounded) {
    EXPECT_FALSE(fsm.tryTransition(CharacterState::Hanging));
    EXPECT_EQ(fsm.currentState(), CharacterState::Grounded);
}

TEST_F(MovementFSMTest, HangingUnreachableFromFalling) {
    fsm.tryTransition(CharacterState::Jumping);
    fsm.tryTransition(CharacterState::Falling);
    EXPECT_FALSE(fsm.tryTransition(CharacterState::Hanging));
    EXPECT_EQ(fsm.currentState(), CharacterState::Falling);
}

TEST_F(MovementFSMTest, SlidingUnreachableFromGrounded) {
    EXPECT_FALSE(fsm.tryTransition(CharacterState::Sliding));
    EXPECT_EQ(fsm.currentState(), CharacterState::Grounded);
}

TEST_F(MovementFSMTest, SlidingUnreachableFromDashing) {
    fsm.tryTransition(CharacterState::Dashing);
    EXPECT_FALSE(fsm.tryTransition(CharacterState::Sliding));
    EXPECT_EQ(fsm.currentState(), CharacterState::Dashing);
}

TEST_F(MovementFSMTest, RagdollUnreachableFromFalling) {
    fsm.tryTransition(CharacterState::Jumping);
    fsm.tryTransition(CharacterState::Falling);
    EXPECT_FALSE(fsm.tryTransition(CharacterState::Ragdoll));
    EXPECT_EQ(fsm.currentState(), CharacterState::Falling);
}

TEST_F(MovementFSMTest, RagdollUnreachableFromFlying) {
    fsm.tryTransition(CharacterState::Flying);
    EXPECT_FALSE(fsm.tryTransition(CharacterState::Ragdoll));
    EXPECT_EQ(fsm.currentState(), CharacterState::Flying);
}

TEST_F(MovementFSMTest, RagdollUnreachableFromBoosting) {
    fsm.tryTransition(CharacterState::Flying);
    fsm.tryTransition(CharacterState::Boosting);
    EXPECT_FALSE(fsm.tryTransition(CharacterState::Ragdoll));
    EXPECT_EQ(fsm.currentState(), CharacterState::Boosting);
}

// Verify none of the 6 unwired states are reachable from any active state
TEST_F(MovementFSMTest, UnwiredStatesUnreachableFromAllActiveStates) {
    const std::vector<CharacterState> unwiredStates = {CharacterState::Climbing,    CharacterState::Swimming,
                                                       CharacterState::WallRunning, CharacterState::Hanging,
                                                       CharacterState::Sliding,     CharacterState::Ragdoll};

    // Test from Grounded
    for (auto state : unwiredStates) {
        MovementFSM fresh;
        EXPECT_FALSE(fresh.tryTransition(state))
            << "Expected rejection from Grounded to " << MovementFSM::stateToString(state);
        EXPECT_EQ(fresh.currentState(), CharacterState::Grounded);
    }

    // Test from Jumping
    for (auto state : unwiredStates) {
        MovementFSM fresh;
        fresh.tryTransition(CharacterState::Jumping);
        EXPECT_FALSE(fresh.tryTransition(state))
            << "Expected rejection from Jumping to " << MovementFSM::stateToString(state);
        EXPECT_EQ(fresh.currentState(), CharacterState::Jumping);
    }

    // Test from Flying
    for (auto state : unwiredStates) {
        MovementFSM fresh;
        fresh.tryTransition(CharacterState::Flying);
        EXPECT_FALSE(fresh.tryTransition(state))
            << "Expected rejection from Flying to " << MovementFSM::stateToString(state);
        EXPECT_EQ(fresh.currentState(), CharacterState::Flying);
    }
}

TEST_F(MovementFSMTest, StateToStringCoversAllStates) {
    EXPECT_EQ(MovementFSM::stateToString(CharacterState::Grounded), "Grounded");
    EXPECT_EQ(MovementFSM::stateToString(CharacterState::Falling), "Falling");
    EXPECT_EQ(MovementFSM::stateToString(CharacterState::Jumping), "Jumping");
    EXPECT_EQ(MovementFSM::stateToString(CharacterState::Climbing), "Climbing");
    EXPECT_EQ(MovementFSM::stateToString(CharacterState::Swimming), "Swimming");
    EXPECT_EQ(MovementFSM::stateToString(CharacterState::WallRunning), "WallRunning");
    EXPECT_EQ(MovementFSM::stateToString(CharacterState::Hanging), "Hanging");
    EXPECT_EQ(MovementFSM::stateToString(CharacterState::Flying), "Flying");
    EXPECT_EQ(MovementFSM::stateToString(CharacterState::Sliding), "Sliding");
    EXPECT_EQ(MovementFSM::stateToString(CharacterState::Ragdoll), "Ragdoll");
    EXPECT_EQ(MovementFSM::stateToString(CharacterState::Dashing), "Dashing");
    EXPECT_EQ(MovementFSM::stateToString(CharacterState::Boosting), "Boosting");
}
