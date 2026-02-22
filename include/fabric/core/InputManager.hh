#pragma once

#include "fabric/core/Event.hh"
#include <SDL3/SDL.h>
#include <array>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace fabric {

// Translates SDL3 events into Fabric EventDispatcher actions.
// Standalone; no main loop wiring yet.
class InputManager {
public:
    InputManager();
    explicit InputManager(EventDispatcher& dispatcher);

    // Bind an action name to an SDL keycode
    void bindKey(const std::string& action, SDL_Keycode key);
    void unbindKey(const std::string& action);

    // Process a single SDL event. Returns true if consumed.
    bool processEvent(const SDL_Event& event);

    // Mouse state
    float mouseX() const;
    float mouseY() const;
    float mouseDeltaX() const;
    float mouseDeltaY() const;
    bool mouseButton(int button) const;

    // Reset per-frame deltas (call at start of frame)
    void beginFrame();

    // Query if action is currently active (key held)
    bool isActionActive(const std::string& action) const;

private:
    EventDispatcher* dispatcher_ = nullptr;
    std::unordered_map<SDL_Keycode, std::string> keyBindings_;
    std::unordered_set<std::string> activeActions_;
    float mouseX_ = 0, mouseY_ = 0;
    float mouseDeltaX_ = 0, mouseDeltaY_ = 0;
    std::array<bool, 5> mouseButtons_ = {};
};

} // namespace fabric
