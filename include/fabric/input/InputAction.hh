#pragma once

#include "fabric/input/InputSource.hh"
#include <string>
#include <vector>

namespace fabric {

/// Digital action states for frame-boundary tracking
enum class ActionState : uint8_t {
    Released,
    JustPressed,
    Held,
    JustReleased
};

/// Binding from an action name to one or more physical input sources.
/// Any source becoming active triggers the action (logical OR).
struct ActionBinding {
    std::string name;
    std::vector<InputSource> sources;

    /// If true, this action fires only once on press (no Held state).
    /// The action transitions directly from JustPressed to JustReleased
    /// on the next frame, regardless of whether the source is still held.
    bool oneShot = false;
};

/// Per-frame snapshot of a single action's state.
/// Computed by InputSystem::evaluate() from raw device state.
struct ActionSnapshot {
    ActionState state = ActionState::Released;

    bool isActive() const { return state == ActionState::JustPressed || state == ActionState::Held; }

    bool justPressed() const { return state == ActionState::JustPressed; }

    bool justReleased() const { return state == ActionState::JustReleased; }
};

} // namespace fabric
