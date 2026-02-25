#include "fabric/core/MovementFSM.hh"
#include "fabric/core/Log.hh"

namespace fabric {

std::string MovementFSM::stateToString(CharacterState state) {
    switch (state) {
        case CharacterState::Grounded:    return "Grounded";
        case CharacterState::Falling:     return "Falling";
        case CharacterState::Jumping:     return "Jumping";
        case CharacterState::Climbing:    return "Climbing";
        case CharacterState::Swimming:    return "Swimming";
        case CharacterState::WallRunning: return "WallRunning";
        case CharacterState::Hanging:     return "Hanging";
        case CharacterState::Flying:      return "Flying";
        case CharacterState::Sliding:     return "Sliding";
        case CharacterState::Ragdoll:     return "Ragdoll";
        case CharacterState::Dashing:     return "Dashing";
        case CharacterState::Boosting:    return "Boosting";
        default:                          return "Unknown";
    }
}

MovementFSM::MovementFSM()
    : sm_(std::make_unique<StateMachine<CharacterState>>(CharacterState::Grounded, stateToString)) {

    // Sprint 5b active transitions: Grounded, Falling, Jumping, Flying, Dashing, Boosting
    sm_->addTransition(CharacterState::Grounded, CharacterState::Jumping);
    sm_->addTransition(CharacterState::Grounded, CharacterState::Falling);
    sm_->addTransition(CharacterState::Grounded, CharacterState::Flying);
    sm_->addTransition(CharacterState::Grounded, CharacterState::Dashing);

    sm_->addTransition(CharacterState::Jumping, CharacterState::Falling);
    sm_->addTransition(CharacterState::Jumping, CharacterState::Flying);

    sm_->addTransition(CharacterState::Falling, CharacterState::Grounded);
    sm_->addTransition(CharacterState::Falling, CharacterState::Flying);

    sm_->addTransition(CharacterState::Flying, CharacterState::Falling);
    sm_->addTransition(CharacterState::Flying, CharacterState::Grounded);
    sm_->addTransition(CharacterState::Flying, CharacterState::Boosting);

    sm_->addTransition(CharacterState::Dashing, CharacterState::Grounded);
    sm_->addTransition(CharacterState::Dashing, CharacterState::Falling);

    sm_->addTransition(CharacterState::Boosting, CharacterState::Flying);
    sm_->addTransition(CharacterState::Boosting, CharacterState::Falling);
}

bool MovementFSM::tryTransition(CharacterState target) {
    CharacterState current = sm_->getState();
    if (!sm_->isValidTransition(current, target)) {
        FABRIC_LOG_DEBUG("Movement transition rejected: {} -> {}", stateToString(current), stateToString(target));
        return false;
    }
    sm_->setState(target);
    return true;
}

CharacterState MovementFSM::currentState() const {
    return sm_->getState();
}

bool MovementFSM::isGrounded() const {
    return sm_->getState() == CharacterState::Grounded;
}

bool MovementFSM::isAirborne() const {
    CharacterState s = sm_->getState();
    return s == CharacterState::Jumping || s == CharacterState::Falling;
}

bool MovementFSM::isFlying() const {
    CharacterState s = sm_->getState();
    return s == CharacterState::Flying || s == CharacterState::Boosting;
}

bool MovementFSM::canDash() const {
    CharacterState s = sm_->getState();
    return s == CharacterState::Grounded;
}

} // namespace fabric
