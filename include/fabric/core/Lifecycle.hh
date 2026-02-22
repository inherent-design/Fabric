#pragma once

#include "fabric/core/StateMachine.hh"
#include <functional>
#include <string>

namespace fabric {

enum class LifecycleState {
  Created,
  Initialized,
  Rendered,
  Updating,
  Suspended,
  Destroyed
};

using LifecycleHook = std::function<void()>;

class LifecycleManager {
public:
  LifecycleManager();

  /// Throws FabricException if the transition is invalid. Self-transitions are no-ops.
  void setState(LifecycleState state);

  LifecycleState getState() const;

  /// Register a hook to be called when transitioning to a specific state.
  /// Returns hook ID for removal. Throws FabricException if hook is null.
  std::string addHook(LifecycleState state, const LifecycleHook& hook);

  /// Register a hook for a specific from->to transition.
  /// Returns hook ID for removal. Throws FabricException if hook is null.
  std::string addTransitionHook(LifecycleState fromState, LifecycleState toState,
                              const LifecycleHook& hook);

  bool removeHook(const std::string& hookId);

  static bool isValidTransition(LifecycleState fromState, LifecycleState toState);

private:
  StateMachine<LifecycleState> sm_;
};

std::string lifecycleStateToString(LifecycleState state);

} // namespace fabric
