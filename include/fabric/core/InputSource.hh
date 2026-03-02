#pragma once

#include <cstdint>
#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_keycode.h>
#include <SDL3/SDL_mouse.h>
#include <string>
#include <variant>

namespace fabric {

/// Physical input source discriminator
enum class InputSourceType : uint8_t {
    Key,
    MouseButton,
    MouseAxis,
    MouseWheel,
    GamepadButton,
    GamepadAxis,
    // Post-MVP:
    // TouchZone,
    // PenButton,
    // PenAxis,
    // SensorAxis,
};

/// Which component of a 2D input (mouse delta, scroll)
enum class InputAxisComponent : uint8_t {
    X,
    Y
};

struct KeySource {
    SDL_Keycode key = SDLK_UNKNOWN;
    bool operator==(const KeySource&) const = default;
};

struct MouseButtonSource {
    uint8_t button = SDL_BUTTON_LEFT;
    bool operator==(const MouseButtonSource&) const = default;
};

struct MouseAxisSource {
    InputAxisComponent component = InputAxisComponent::X;
    bool operator==(const MouseAxisSource&) const = default;
};

struct MouseWheelSource {
    InputAxisComponent component = InputAxisComponent::Y;
    bool operator==(const MouseWheelSource&) const = default;
};

struct GamepadButtonSource {
    SDL_GamepadButton button = SDL_GAMEPAD_BUTTON_INVALID;
    bool operator==(const GamepadButtonSource&) const = default;
};

struct GamepadAxisSource {
    SDL_GamepadAxis axis = SDL_GAMEPAD_AXIS_INVALID;
    bool operator==(const GamepadAxisSource&) const = default;
};

// Post-MVP stubs (forward declarations for future device support)
struct TouchSource;
struct PenSource;
struct SensorSource;

/// A physical input source. Tagged variant over all supported device inputs.
/// Each source knows its type and can report a human-readable name.
/// Interpretation (digital vs analog) depends on whether the source is
/// bound to an ActionBinding or AxisBinding.
using InputSource = std::variant<KeySource, MouseButtonSource, MouseAxisSource, MouseWheelSource, GamepadButtonSource,
                                 GamepadAxisSource>;

/// Returns a human-readable name for an InputSource (for UI/config display).
std::string inputSourceName(const InputSource& source);

/// Returns the InputSourceType tag for a given source variant.
InputSourceType inputSourceType(const InputSource& source);

} // namespace fabric
