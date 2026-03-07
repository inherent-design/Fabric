#include "fabric/core/InputRouter.hh"
#include "fabric/core/Log.hh"

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/Element.h>

#include <unordered_map>

namespace fabric {

InputRouter::InputRouter(InputManager& inputMgr) : inputMgr_(inputMgr) {}

void InputRouter::setEventCaptureHook(std::function<void(const SDL_Event&)> hook) {
    eventCaptureHook_ = std::move(hook);
}

void InputRouter::setFrameHook(std::function<void()> hook) {
    frameHook_ = std::move(hook);
}

void InputRouter::setMode(InputMode mode) {
    if (mode_ != mode) {
        FABRIC_LOG_DEBUG("InputMode: {} -> {}", static_cast<int>(mode_), static_cast<int>(mode));
        mode_ = mode;
    }
}

InputMode InputRouter::mode() const {
    return mode_;
}

void InputRouter::registerKeyCallback(SDL_Keycode key, std::function<void()> cb) {
    keyCallbacks_[key] = std::move(cb);
}

void InputRouter::registerKeyCallback(SDL_Keycode key, SDL_Keymod mod, std::function<void()> cb) {
    uint32_t combined = (static_cast<uint32_t>(key) << 16) | static_cast<uint32_t>(mod);
    modKeyCallbacks_[combined] = std::move(cb);
}

void InputRouter::unregisterKeyCallback(SDL_Keycode key) {
    keyCallbacks_.erase(key);
}

bool InputRouter::routeEvent(const SDL_Event& event, Rml::Context* rmlContext) {
    // Capture event for recording (before any routing consumes it)
    if (eventCaptureHook_) {
        eventCaptureHook_(event);
    }

    // Backtick toggles developer console (intercept before any other routing)
    if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat && event.key.key == SDLK_GRAVE) {
        if (consoleToggleCallback_) {
            consoleToggleCallback_();
            return true;
        }
    }

    // Escape: use callback if registered, fall back to direct toggle
    if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat && event.key.key == SDLK_ESCAPE) {
        if (escapeCallback_) {
            escapeCallback_();
        } else {
            toggleUIMode();
        }
        return true;
    }

    // Key callbacks: fire on non-repeat key-down, suppressed in UIOnly mode
    if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat && mode_ != InputMode::UIOnly) {
        // Check modifier-aware callbacks first (higher priority)
        // Iterate to find matching key + any required modifier
        for (const auto& [combined, cb] : modKeyCallbacks_) {
            SDL_Keycode key = static_cast<SDL_Keycode>(combined >> 16);
            SDL_Keymod mod = static_cast<SDL_Keymod>(combined & 0xFFFF);
            // Match if key matches AND any of the required modifiers are pressed
            if (event.key.key == key && (event.key.mod & mod) != 0) {
                cb();
                return true;
            }
        }
        // Fall back to plain key callbacks
        auto it = keyCallbacks_.find(event.key.key);
        if (it != keyCallbacks_.end()) {
            it->second();
            return true;
        }
    }

    switch (mode_) {
        case InputMode::GameOnly:
            return forwardToGame(event);

        case InputMode::UIOnly:
            if (rmlContext) {
                return forwardToRmlUI(event, rmlContext);
            }
            return false;

        case InputMode::GameAndUI: {
            if (!rmlContext) {
                return forwardToGame(event);
            }

            // Text input always goes to RmlUI in this mode
            if (event.type == SDL_EVENT_TEXT_INPUT) {
                return forwardToRmlUI(event, rmlContext);
            }

            // Mouse events: UI first, game as fallback
            if (event.type == SDL_EVENT_MOUSE_MOTION || event.type == SDL_EVENT_MOUSE_BUTTON_DOWN ||
                event.type == SDL_EVENT_MOUSE_BUTTON_UP || event.type == SDL_EVENT_MOUSE_WHEEL) {

                // Forward to RmlUI first
                forwardToRmlUI(event, rmlContext);

                // Check if RmlUI has a hovered element beyond the body
                Rml::Element* hover = rmlContext->GetHoverElement();
                if (hover && !isBodyElement(hover)) {
                    return true; // consumed by UI
                }

                // UI didn't claim it, forward to game
                return forwardToGame(event);
            }

            // Keyboard: check if RmlUI has focus on a text input
            if (event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_KEY_UP) {
                Rml::Element* focus = rmlContext->GetFocusElement();
                if (focus && !isBodyElement(focus)) {
                    return forwardToRmlUI(event, rmlContext);
                }
                return forwardToGame(event);
            }

            return forwardToGame(event);
        }
    }

    return false;
}

void InputRouter::beginFrame() {
    inputMgr_.beginFrame();

    if (frameHook_) {
        frameHook_();
    }
}

void InputRouter::toggleUIMode() {
    if (mode_ == InputMode::GameOnly) {
        setMode(InputMode::UIOnly);
    } else if (mode_ == InputMode::UIOnly) {
        setMode(InputMode::GameOnly);
    }
    // GameAndUI stays as-is on toggle
}

// Convert window coordinates to pixel coordinates for HiDPI support
std::pair<int, int> InputRouter::windowToPixelCoords(float winX, float winY) const {
    if (!window_) {
        return {static_cast<int>(winX), static_cast<int>(winY)};
    }

    int winW, winH, pixelW, pixelH;
    SDL_GetWindowSize(window_, &winW, &winH);
    SDL_GetWindowSizeInPixels(window_, &pixelW, &pixelH);

    if (winW == 0 || winH == 0) {
        return {static_cast<int>(winX), static_cast<int>(winY)};
    }

    float scaleX = static_cast<float>(pixelW) / static_cast<float>(winW);
    float scaleY = static_cast<float>(pixelH) / static_cast<float>(winH);

    return {static_cast<int>(winX * scaleX), static_cast<int>(winY * scaleY)};
}

bool InputRouter::forwardToRmlUI(const SDL_Event& event, Rml::Context* ctx) {
    switch (event.type) {
        case SDL_EVENT_KEY_DOWN: {
            auto rmlKey = sdlKeyToRmlKey(event.key.key);
            int rmlMod = sdlModToRmlMod(event.key.mod);
            return ctx->ProcessKeyDown(rmlKey, rmlMod);
        }

        case SDL_EVENT_KEY_UP: {
            auto rmlKey = sdlKeyToRmlKey(event.key.key);
            int rmlMod = sdlModToRmlMod(event.key.mod);
            return ctx->ProcessKeyUp(rmlKey, rmlMod);
        }

        case SDL_EVENT_MOUSE_MOTION: {
            int rmlMod = sdlModToRmlMod(SDL_GetModState());
            auto [px, py] = windowToPixelCoords(event.motion.x, event.motion.y);
            return ctx->ProcessMouseMove(px, py, rmlMod);
        }

        case SDL_EVENT_MOUSE_BUTTON_DOWN: {
            int rmlMod = sdlModToRmlMod(SDL_GetModState());
            int rmlButton = sdlMouseButtonToRml(event.button.button);
            // Update mouse position on click for accurate hit testing
            auto [px, py] = windowToPixelCoords(event.button.x, event.button.y);
            FABRIC_LOG_INFO("InputRouter: Mouse button {} down at window({}, {}) -> pixel({}, {})", rmlButton,
                            event.button.x, event.button.y, px, py);
            ctx->ProcessMouseMove(px, py, rmlMod);
            bool result = ctx->ProcessMouseButtonDown(rmlButton, rmlMod);
            FABRIC_LOG_INFO("InputRouter: ProcessMouseButtonDown returned {}", result);
            return result;
        }

        case SDL_EVENT_MOUSE_BUTTON_UP: {
            int rmlMod = sdlModToRmlMod(SDL_GetModState());
            int rmlButton = sdlMouseButtonToRml(event.button.button);
            auto [px, py] = windowToPixelCoords(event.button.x, event.button.y);
            ctx->ProcessMouseMove(px, py, rmlMod);
            return ctx->ProcessMouseButtonUp(rmlButton, rmlMod);
        }

        case SDL_EVENT_MOUSE_WHEEL: {
            int rmlMod = sdlModToRmlMod(SDL_GetModState());
            return ctx->ProcessMouseWheel(event.wheel.y, rmlMod);
        }

        case SDL_EVENT_TEXT_INPUT: {
            if (event.text.text) {
                return ctx->ProcessTextInput(Rml::String(event.text.text));
            }
            return false;
        }

        default:
            return false;
    }
}

bool InputRouter::forwardToGame(const SDL_Event& event) {
    return inputMgr_.processEvent(event);
}

int InputRouter::sdlMouseButtonToRml(Uint8 button) {
    // RmlUI convention: left=0, right=1, middle=2
    switch (button) {
        case SDL_BUTTON_LEFT:
            return 0;
        case SDL_BUTTON_RIGHT:
            return 1;
        case SDL_BUTTON_MIDDLE:
            return 2;
        case SDL_BUTTON_X1:
            return 3;
        case SDL_BUTTON_X2:
            return 4;
        default:
            return 0;
    }
}

bool InputRouter::isBodyElement(Rml::Element* element) {
    if (!element)
        return true;
    return element->GetTagName() == "body";
}

// SDL keycode -> RmlUI KeyIdentifier mapping
Rml::Input::KeyIdentifier InputRouter::sdlKeyToRmlKey(SDL_Keycode key) {
    // A-Z: SDL keycodes for lowercase letters are 0x61-0x7a
    if (key >= SDLK_A && key <= SDLK_Z) {
        return static_cast<Rml::Input::KeyIdentifier>(Rml::Input::KI_A + (key - SDLK_A));
    }

    // 0-9: SDL keycodes are 0x30-0x39
    if (key >= SDLK_0 && key <= SDLK_9) {
        return static_cast<Rml::Input::KeyIdentifier>(Rml::Input::KI_0 + (key - SDLK_0));
    }

    // F1-F12
    if (key >= SDLK_F1 && key <= SDLK_F12) {
        return static_cast<Rml::Input::KeyIdentifier>(Rml::Input::KI_F1 + (key - SDLK_F1));
    }

    // Numpad 1-9 (SDL orders KP_1..KP_9, then KP_0)
    if (key >= SDLK_KP_1 && key <= SDLK_KP_9) {
        return static_cast<Rml::Input::KeyIdentifier>(Rml::Input::KI_NUMPAD1 + (key - SDLK_KP_1));
    }
    if (key == SDLK_KP_0) {
        return Rml::Input::KI_NUMPAD0;
    }

    // Individual keys
    static const std::unordered_map<SDL_Keycode, Rml::Input::KeyIdentifier> keyMap = {
        {SDLK_SPACE, Rml::Input::KI_SPACE},
        {SDLK_RETURN, Rml::Input::KI_RETURN},
        {SDLK_KP_ENTER, Rml::Input::KI_NUMPADENTER},
        {SDLK_ESCAPE, Rml::Input::KI_ESCAPE},
        {SDLK_BACKSPACE, Rml::Input::KI_BACK},
        {SDLK_TAB, Rml::Input::KI_TAB},
        {SDLK_DELETE, Rml::Input::KI_DELETE},
        {SDLK_INSERT, Rml::Input::KI_INSERT},
        {SDLK_HOME, Rml::Input::KI_HOME},
        {SDLK_END, Rml::Input::KI_END},
        {SDLK_PAGEUP, Rml::Input::KI_PRIOR},
        {SDLK_PAGEDOWN, Rml::Input::KI_NEXT},
        {SDLK_LEFT, Rml::Input::KI_LEFT},
        {SDLK_RIGHT, Rml::Input::KI_RIGHT},
        {SDLK_UP, Rml::Input::KI_UP},
        {SDLK_DOWN, Rml::Input::KI_DOWN},
        {SDLK_LSHIFT, Rml::Input::KI_LSHIFT},
        {SDLK_RSHIFT, Rml::Input::KI_RSHIFT},
        {SDLK_LCTRL, Rml::Input::KI_LCONTROL},
        {SDLK_RCTRL, Rml::Input::KI_RCONTROL},
        {SDLK_LALT, Rml::Input::KI_LMENU},
        {SDLK_RALT, Rml::Input::KI_RMENU},
        {SDLK_LGUI, Rml::Input::KI_LMETA},
        {SDLK_RGUI, Rml::Input::KI_RMETA},
        {SDLK_CAPSLOCK, Rml::Input::KI_CAPITAL},
        {SDLK_NUMLOCKCLEAR, Rml::Input::KI_NUMLOCK},
        {SDLK_SCROLLLOCK, Rml::Input::KI_SCROLL},
        {SDLK_PAUSE, Rml::Input::KI_PAUSE},
        {SDLK_PRINTSCREEN, Rml::Input::KI_SNAPSHOT},
        {SDLK_SEMICOLON, Rml::Input::KI_OEM_1},
        {SDLK_EQUALS, Rml::Input::KI_OEM_PLUS},
        {SDLK_COMMA, Rml::Input::KI_OEM_COMMA},
        {SDLK_MINUS, Rml::Input::KI_OEM_MINUS},
        {SDLK_PERIOD, Rml::Input::KI_OEM_PERIOD},
        {SDLK_SLASH, Rml::Input::KI_OEM_2},
        {SDLK_GRAVE, Rml::Input::KI_OEM_3},
        {SDLK_LEFTBRACKET, Rml::Input::KI_OEM_4},
        {SDLK_BACKSLASH, Rml::Input::KI_OEM_5},
        {SDLK_RIGHTBRACKET, Rml::Input::KI_OEM_6},
        {SDLK_APOSTROPHE, Rml::Input::KI_OEM_7},
        {SDLK_KP_MULTIPLY, Rml::Input::KI_MULTIPLY},
        {SDLK_KP_PLUS, Rml::Input::KI_ADD},
        {SDLK_KP_MINUS, Rml::Input::KI_SUBTRACT},
        {SDLK_KP_PERIOD, Rml::Input::KI_DECIMAL},
        {SDLK_KP_DIVIDE, Rml::Input::KI_DIVIDE},
    };

    auto it = keyMap.find(key);
    if (it != keyMap.end()) {
        return it->second;
    }

    return Rml::Input::KI_UNKNOWN;
}

int InputRouter::sdlModToRmlMod(SDL_Keymod mod) {
    int rmlMod = 0;
    if (mod & SDL_KMOD_CTRL)
        rmlMod |= Rml::Input::KM_CTRL;
    if (mod & SDL_KMOD_SHIFT)
        rmlMod |= Rml::Input::KM_SHIFT;
    if (mod & SDL_KMOD_ALT)
        rmlMod |= Rml::Input::KM_ALT;
    if (mod & SDL_KMOD_GUI)
        rmlMod |= Rml::Input::KM_META;
    if (mod & SDL_KMOD_CAPS)
        rmlMod |= Rml::Input::KM_CAPSLOCK;
    if (mod & SDL_KMOD_NUM)
        rmlMod |= Rml::Input::KM_NUMLOCK;
    if (mod & SDL_KMOD_SCROLL)
        rmlMod |= Rml::Input::KM_SCROLLLOCK;
    return rmlMod;
}

} // namespace fabric
