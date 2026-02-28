#include "fabric/core/MovementFSM.hh"
#include <gtest/gtest.h>

using namespace fabric;

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
