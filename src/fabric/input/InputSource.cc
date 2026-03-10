#include "fabric/input/InputSource.hh"
#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_keyboard.h>

namespace fabric {

InputSourceType inputSourceType(const InputSource& source) {
    return std::visit(
        [](auto&& s) -> InputSourceType {
            using T = std::decay_t<decltype(s)>;
            if constexpr (std::is_same_v<T, KeySource>)
                return InputSourceType::Key;
            else if constexpr (std::is_same_v<T, MouseButtonSource>)
                return InputSourceType::MouseButton;
            else if constexpr (std::is_same_v<T, MouseAxisSource>)
                return InputSourceType::MouseAxis;
            else if constexpr (std::is_same_v<T, MouseWheelSource>)
                return InputSourceType::MouseWheel;
            else if constexpr (std::is_same_v<T, GamepadButtonSource>)
                return InputSourceType::GamepadButton;
            else if constexpr (std::is_same_v<T, GamepadAxisSource>)
                return InputSourceType::GamepadAxis;
        },
        source);
}

std::string inputSourceName(const InputSource& source) {
    return std::visit(
        [](auto&& s) -> std::string {
            using T = std::decay_t<decltype(s)>;
            if constexpr (std::is_same_v<T, KeySource>) {
                const char* name = SDL_GetKeyName(s.key);
                return (name && name[0] != '\0') ? name : "Unknown Key";
            } else if constexpr (std::is_same_v<T, MouseButtonSource>) {
                switch (s.button) {
                    case SDL_BUTTON_LEFT:
                        return "Mouse Left";
                    case SDL_BUTTON_MIDDLE:
                        return "Mouse Middle";
                    case SDL_BUTTON_RIGHT:
                        return "Mouse Right";
                    case SDL_BUTTON_X1:
                        return "Mouse X1";
                    case SDL_BUTTON_X2:
                        return "Mouse X2";
                    default:
                        return "Mouse Button " + std::to_string(s.button);
                }
            } else if constexpr (std::is_same_v<T, MouseAxisSource>) {
                return s.component == InputAxisComponent::X ? "Mouse X" : "Mouse Y";
            } else if constexpr (std::is_same_v<T, MouseWheelSource>) {
                return s.component == InputAxisComponent::X ? "Scroll X" : "Scroll Y";
            } else if constexpr (std::is_same_v<T, GamepadButtonSource>) {
                const char* name = SDL_GetGamepadStringForButton(s.button);
                return name ? std::string("Gamepad ") + name : "Gamepad Button";
            } else if constexpr (std::is_same_v<T, GamepadAxisSource>) {
                const char* name = SDL_GetGamepadStringForAxis(s.axis);
                return name ? std::string("Gamepad ") + name : "Gamepad Axis";
            }
        },
        source);
}

} // namespace fabric
