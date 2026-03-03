#pragma once

#include "fabric/core/StateMachine.hh"
#include "recurse/gameplay/CharacterTypes.hh"
#include <memory>
#include <string>

namespace recurse {

// Engine types imported from fabric:: namespace
using fabric::StateMachine;

class MovementFSM {
  public:
    MovementFSM();

    bool tryTransition(CharacterState target);
    CharacterState currentState() const;

    bool isGrounded() const;
    bool isAirborne() const;
    bool isFlying() const;
    bool canDash() const;

    static std::string stateToString(CharacterState state);

  private:
    std::unique_ptr<StateMachine<CharacterState>> sm_;
};

} // namespace recurse
