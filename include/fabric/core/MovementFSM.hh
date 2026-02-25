#pragma once

#include "fabric/core/CharacterTypes.hh"
#include "fabric/core/StateMachine.hh"
#include <memory>
#include <string>

namespace fabric {

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

} // namespace fabric
