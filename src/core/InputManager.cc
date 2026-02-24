#include "fabric/core/InputManager.hh"
#include "fabric/core/Log.hh"

namespace fabric {

InputManager::InputManager() = default;

InputManager::InputManager(EventDispatcher& dispatcher) : dispatcher_(&dispatcher) {}

void InputManager::bindKey(const std::string& action, SDL_Keycode key) {
    keyBindings_[key] = action;
}

void InputManager::unbindKey(const std::string& action) {
    for (auto it = keyBindings_.begin(); it != keyBindings_.end();) {
        if (it->second == action) {
            it = keyBindings_.erase(it);
        } else {
            ++it;
        }
    }
}

bool InputManager::processEvent(const SDL_Event& event) {
    switch (event.type) {
        case SDL_EVENT_KEY_DOWN: {
            if (event.key.repeat)
                return false;

            auto it = keyBindings_.find(event.key.key);
            if (it == keyBindings_.end())
                return false;

            const auto& action = it->second;
            activeActions_.insert(action);

            if (dispatcher_) {
                Event e(action, "InputManager");
                dispatcher_->dispatchEvent(e);
            }
            return true;
        }

        case SDL_EVENT_KEY_UP: {
            auto it = keyBindings_.find(event.key.key);
            if (it == keyBindings_.end())
                return false;

            const auto& action = it->second;
            activeActions_.erase(action);

            if (dispatcher_) {
                Event e(action + ":released", "InputManager");
                dispatcher_->dispatchEvent(e);
            }
            return true;
        }

        case SDL_EVENT_MOUSE_MOTION: {
            mouseX_ = event.motion.x;
            mouseY_ = event.motion.y;
            mouseDeltaX_ += event.motion.xrel;
            mouseDeltaY_ += event.motion.yrel;
            return true;
        }

        case SDL_EVENT_MOUSE_BUTTON_DOWN: {
            int idx = event.button.button - 1;
            if (idx >= 0 && idx < 5) {
                mouseButtons_[static_cast<size_t>(idx)] = true;
            }
            return true;
        }

        case SDL_EVENT_MOUSE_BUTTON_UP: {
            int idx = event.button.button - 1;
            if (idx >= 0 && idx < 5) {
                mouseButtons_[static_cast<size_t>(idx)] = false;
            }
            return true;
        }

        default:
            return false;
    }
}

float InputManager::mouseX() const {
    return mouseX_;
}
float InputManager::mouseY() const {
    return mouseY_;
}
float InputManager::mouseDeltaX() const {
    return mouseDeltaX_;
}
float InputManager::mouseDeltaY() const {
    return mouseDeltaY_;
}

bool InputManager::mouseButton(int button) const {
    int idx = button - 1;
    if (idx >= 0 && idx < 5) {
        return mouseButtons_[static_cast<size_t>(idx)];
    }
    return false;
}

void InputManager::beginFrame() {
    mouseDeltaX_ = 0;
    mouseDeltaY_ = 0;
}

bool InputManager::isActionActive(const std::string& action) const {
    return activeActions_.count(action) > 0;
}

} // namespace fabric
