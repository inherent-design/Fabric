#include "fabric/core/InputSystem.hh"

#include "fabric/core/Event.hh"
#include "fabric/core/Log.hh"
#include <algorithm>
#include <cmath>

namespace fabric {

InputSystem::InputSystem() = default;

InputSystem::InputSystem(EventDispatcher& dispatcher) : dispatcher_(&dispatcher) {}

// --- System lifecycle ---

void InputSystem::doInit(AppContext& ctx) {
    (void)ctx;
}

void InputSystem::doShutdown() {
    for (auto& device : connectedDevices_) {
        if (device.gamepad) {
            SDL_CloseGamepad(device.gamepad);
            device.gamepad = nullptr;
        }
    }
    connectedDevices_.clear();
    deviceState_.gamepadConnected = false;
}

void InputSystem::update(AppContext& ctx, float dt) {
    (void)ctx;
    (void)dt;
    evaluate();
}

// --- Frame lifecycle ---

void InputSystem::beginFrame() {
    deviceState_.mouseDeltaX = 0;
    deviceState_.mouseDeltaY = 0;
    deviceState_.scrollDeltaX = 0;
    deviceState_.scrollDeltaY = 0;
}

bool InputSystem::processEvent(const SDL_Event& event) {
    switch (event.type) {
        case SDL_EVENT_KEY_DOWN:
            if (!event.key.repeat) {
                deviceState_.keysDown.insert(event.key.key);
            }
            return true;

        case SDL_EVENT_KEY_UP:
            deviceState_.keysDown.erase(event.key.key);
            return true;

        case SDL_EVENT_MOUSE_MOTION:
            deviceState_.mouseX = event.motion.x;
            deviceState_.mouseY = event.motion.y;
            deviceState_.mouseDeltaX += event.motion.xrel;
            deviceState_.mouseDeltaY += event.motion.yrel;
            return true;

        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            if (event.button.button >= 1 && event.button.button <= 5) {
                deviceState_.mouseButtons[event.button.button - 1] = true;
            }
            return true;

        case SDL_EVENT_MOUSE_BUTTON_UP:
            if (event.button.button >= 1 && event.button.button <= 5) {
                deviceState_.mouseButtons[event.button.button - 1] = false;
            }
            return true;

        case SDL_EVENT_MOUSE_WHEEL:
            deviceState_.scrollDeltaX += event.wheel.x;
            deviceState_.scrollDeltaY += event.wheel.y;
            return true;

        case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
            if (event.gbutton.button < SDL_GAMEPAD_BUTTON_COUNT) {
                deviceState_.gamepadButtons[event.gbutton.button] = true;
            }
            return true;

        case SDL_EVENT_GAMEPAD_BUTTON_UP:
            if (event.gbutton.button < SDL_GAMEPAD_BUTTON_COUNT) {
                deviceState_.gamepadButtons[event.gbutton.button] = false;
            }
            return true;

        case SDL_EVENT_GAMEPAD_AXIS_MOTION:
            if (event.gaxis.axis < SDL_GAMEPAD_AXIS_COUNT) {
                // Normalize int16 [-32768, 32767] to float [-1, 1]
                deviceState_.gamepadAxes[event.gaxis.axis] = static_cast<float>(event.gaxis.value) / 32767.0f;
            }
            return true;

        case SDL_EVENT_GAMEPAD_ADDED:
            handleGamepadAdded(event.gdevice.which);
            return true;

        case SDL_EVENT_GAMEPAD_REMOVED:
            handleGamepadRemoved(event.gdevice.which);
            return true;

        default:
            return false;
    }
}

void InputSystem::evaluate() {
    // Snapshot current active actions before clearing
    std::unordered_set<std::string> currentActive;

    actionSnapshots_.clear();
    axisValues_.clear();

    // Track which action/axis names have been consumed by higher-priority contexts
    std::unordered_set<std::string> consumedActions;
    std::unordered_set<std::string> consumedAxes;

    // Contexts are sorted highest priority first
    for (const auto& ctx : contexts_) {
        if (!ctx->enabled())
            continue;

        // Evaluate actions
        for (const auto& binding : ctx->actions()) {
            if (consumedActions.count(binding.name))
                continue;

            bool active = evaluateAction(binding);

            // oneShot suppression: if already fired and source still held, suppress
            if (binding.oneShot && active && oneShotFired_.count(binding.name)) {
                ActionSnapshot snap;
                snap.state = ActionState::Released;
                actionSnapshots_.try_emplace(binding.name, snap);
                if (ctx->consumeInput()) {
                    consumedActions.insert(binding.name);
                }
                continue;
            }

            // Clear fired flag when source is released
            if (binding.oneShot && !active) {
                oneShotFired_.erase(binding.name);
            }

            if (active) {
                currentActive.insert(binding.name);
            }

            // Determine state based on previous frame
            ActionSnapshot snap;
            bool wasActive = prevActiveActions_.count(binding.name) > 0;

            if (active && !wasActive) {
                snap.state = ActionState::JustPressed;
            } else if (active && wasActive) {
                snap.state = binding.oneShot ? ActionState::JustReleased : ActionState::Held;
                if (binding.oneShot) {
                    oneShotFired_.insert(binding.name);
                }
            } else if (!active && wasActive) {
                snap.state = ActionState::JustReleased;
            } else {
                snap.state = ActionState::Released;
            }

            // Only store if not already set by a higher-priority context
            actionSnapshots_.try_emplace(binding.name, snap);

            if (ctx->consumeInput()) {
                consumedActions.insert(binding.name);
            }
        }

        // Evaluate axes
        for (const auto& binding : ctx->axes()) {
            if (consumedAxes.count(binding.name))
                continue;

            float value = evaluateAxis(binding);

            axisValues_.try_emplace(binding.name, value);

            if (ctx->consumeInput()) {
                consumedAxes.insert(binding.name);
            }
        }
    }

    // Fire EventDispatcher events for state transitions
    if (dispatcher_) {
        for (const auto& [name, snap] : actionSnapshots_) {
            if (snap.state == ActionState::JustPressed) {
                Event ev(name, "InputSystem");
                dispatcher_->dispatchEvent(ev);
            } else if (snap.state == ActionState::JustReleased) {
                Event ev(name + ":released", "InputSystem");
                dispatcher_->dispatchEvent(ev);
            }
        }
    }

    // For oneShot actions that were JustPressed last frame, remove from active
    // set so they transition to Released next frame
    for (const auto& [name, snap] : actionSnapshots_) {
        if (snap.state == ActionState::JustReleased) {
            currentActive.erase(name);
        }
    }

    prevActiveActions_ = std::move(currentActive);
}

// --- Context stack ---

void InputSystem::pushContext(std::shared_ptr<InputContext> context) {
    contexts_.push_back(std::move(context));
    // Sort by priority descending (highest first)
    std::stable_sort(contexts_.begin(), contexts_.end(),
                     [](const auto& a, const auto& b) { return a->priority() > b->priority(); });
}

bool InputSystem::popContext(const std::string& name) {
    auto it =
        std::find_if(contexts_.begin(), contexts_.end(), [&name](const auto& ctx) { return ctx->name() == name; });
    if (it == contexts_.end())
        return false;
    contexts_.erase(it);
    return true;
}

void InputSystem::clearContexts() {
    contexts_.clear();
}

InputContext* InputSystem::findContext(const std::string& name) {
    auto it =
        std::find_if(contexts_.begin(), contexts_.end(), [&name](const auto& ctx) { return ctx->name() == name; });
    return it != contexts_.end() ? it->get() : nullptr;
}

const InputContext* InputSystem::findContext(const std::string& name) const {
    auto it =
        std::find_if(contexts_.begin(), contexts_.end(), [&name](const auto& ctx) { return ctx->name() == name; });
    return it != contexts_.end() ? it->get() : nullptr;
}

const std::vector<std::shared_ptr<InputContext>>& InputSystem::contexts() const {
    return contexts_;
}

// --- Query API ---

ActionSnapshot InputSystem::actionState(const std::string& actionName) const {
    auto it = actionSnapshots_.find(actionName);
    if (it != actionSnapshots_.end())
        return it->second;
    return {};
}

bool InputSystem::isActionActive(const std::string& actionName) const {
    return actionState(actionName).isActive();
}

bool InputSystem::isActionJustPressed(const std::string& actionName) const {
    return actionState(actionName).justPressed();
}

bool InputSystem::isActionJustReleased(const std::string& actionName) const {
    return actionState(actionName).justReleased();
}

float InputSystem::getAxisValue(const std::string& axisName) const {
    auto it = axisValues_.find(axisName);
    if (it != axisValues_.end())
        return it->second;
    return 0.0f;
}

// --- Raw device state ---

const DeviceState& InputSystem::deviceState() const {
    return deviceState_;
}

// --- Device management ---

void InputSystem::onDeviceConnected(DeviceCallback callback) {
    onConnected_.push_back(std::move(callback));
}

void InputSystem::onDeviceDisconnected(DeviceCallback callback) {
    onDisconnected_.push_back(std::move(callback));
}

const std::vector<DeviceInfo>& InputSystem::connectedDevices() const {
    return connectedDevices_;
}

void InputSystem::rumble(uint16_t lowFreq, uint16_t highFreq, uint32_t durationMs) {
    for (auto& device : connectedDevices_) {
        if (device.gamepad) {
            SDL_RumbleGamepad(device.gamepad, lowFreq, highFreq, durationMs);
            return;
        }
    }
}

// --- RmlUI integration ---

bool InputSystem::routeToRmlUI(const SDL_Event& event, Rml::Context* rmlContext) {
    // Phase 1 stub: RmlUI bridge migration happens in Phase 2.
    // InputRouter continues to handle RmlUI forwarding during migration.
    (void)event;
    (void)rmlContext;
    return false;
}

// --- Recording ---

void InputSystem::setRecorder(InputRecorder* recorder) {
    recorder_ = recorder;
}

// --- EventDispatcher ---

void InputSystem::setDispatcher(EventDispatcher* dispatcher) {
    dispatcher_ = dispatcher;
}

// --- Internal: hot-plug ---

void InputSystem::handleGamepadAdded(SDL_JoystickID id) {
    SDL_Gamepad* gp = SDL_OpenGamepad(id);
    if (!gp) {
        FABRIC_LOG_WARN("Failed to open gamepad {}: {}", id, SDL_GetError());
        return;
    }
    DeviceInfo info;
    info.id = id;
    const char* gpName = SDL_GetGamepadName(gp);
    info.name = gpName ? gpName : "Unknown";
    info.isGamepad = true;
    info.gamepad = gp;
    connectedDevices_.push_back(info);
    deviceState_.gamepadConnected = true;
    FABRIC_LOG_INFO("Gamepad connected: {} ({})", info.name, id);
    for (auto& cb : onConnected_) {
        cb(info);
    }
}

void InputSystem::handleGamepadRemoved(SDL_JoystickID id) {
    auto it = std::find_if(connectedDevices_.begin(), connectedDevices_.end(),
                           [id](const DeviceInfo& d) { return d.id == id; });
    if (it != connectedDevices_.end()) {
        FABRIC_LOG_INFO("Gamepad disconnected: {} ({})", it->name, id);
        DeviceInfo info = *it;
        if (it->gamepad) {
            SDL_CloseGamepad(it->gamepad);
        }
        connectedDevices_.erase(it);
        deviceState_.gamepadConnected = !connectedDevices_.empty();
        deviceState_.gamepadButtons.fill(false);
        deviceState_.gamepadAxes.fill(0.0f);
        for (auto& cb : onDisconnected_) {
            cb(info);
        }
    }
}

// --- Internal: binding evaluation ---

bool InputSystem::evaluateAction(const ActionBinding& binding) const {
    // Logical OR: any source active triggers the action
    for (const auto& source : binding.sources) {
        if (readSourceDigital(source))
            return true;
    }
    return false;
}

float InputSystem::evaluateAxis(const AxisBinding& binding) const {
    // Priority: first non-zero source wins
    float rawValue = 0.0f;
    for (const auto& source : binding.sources) {
        float val;
        if (source.useKeyPair) {
            val = readKeyPair(source.keyPair) * source.scale;
        } else {
            val = readSourceAnalog(source.source) * source.scale;
        }
        if (val != 0.0f) {
            rawValue = val;
            break;
        }
    }
    return processAxisValue(rawValue, binding);
}

float InputSystem::readSourceAnalog(const InputSource& source) const {
    return std::visit(
        [this](const auto& s) -> float {
            using T = std::decay_t<decltype(s)>;
            if constexpr (std::is_same_v<T, KeySource>) {
                return deviceState_.keysDown.count(s.key) ? 1.0f : 0.0f;
            } else if constexpr (std::is_same_v<T, MouseButtonSource>) {
                if (s.button >= 1 && s.button <= 5)
                    return deviceState_.mouseButtons[s.button - 1] ? 1.0f : 0.0f;
                return 0.0f;
            } else if constexpr (std::is_same_v<T, MouseAxisSource>) {
                return s.component == InputAxisComponent::X ? deviceState_.mouseDeltaX : deviceState_.mouseDeltaY;
            } else if constexpr (std::is_same_v<T, MouseWheelSource>) {
                return s.component == InputAxisComponent::X ? deviceState_.scrollDeltaX : deviceState_.scrollDeltaY;
            } else if constexpr (std::is_same_v<T, GamepadButtonSource>) {
                if (s.button >= 0 && s.button < SDL_GAMEPAD_BUTTON_COUNT)
                    return deviceState_.gamepadButtons[s.button] ? 1.0f : 0.0f;
                return 0.0f;
            } else if constexpr (std::is_same_v<T, GamepadAxisSource>) {
                if (s.axis >= 0 && s.axis < SDL_GAMEPAD_AXIS_COUNT)
                    return deviceState_.gamepadAxes[s.axis];
                return 0.0f;
            } else {
                return 0.0f;
            }
        },
        source);
}

bool InputSystem::readSourceDigital(const InputSource& source) const {
    return std::visit(
        [this](const auto& s) -> bool {
            using T = std::decay_t<decltype(s)>;
            if constexpr (std::is_same_v<T, KeySource>) {
                return deviceState_.keysDown.count(s.key) > 0;
            } else if constexpr (std::is_same_v<T, MouseButtonSource>) {
                if (s.button >= 1 && s.button <= 5)
                    return deviceState_.mouseButtons[s.button - 1];
                return false;
            } else if constexpr (std::is_same_v<T, MouseAxisSource>) {
                return false; // Mouse axes are analog only
            } else if constexpr (std::is_same_v<T, MouseWheelSource>) {
                return false; // Scroll is analog only
            } else if constexpr (std::is_same_v<T, GamepadButtonSource>) {
                if (s.button >= 0 && s.button < SDL_GAMEPAD_BUTTON_COUNT)
                    return deviceState_.gamepadButtons[s.button];
                return false;
            } else if constexpr (std::is_same_v<T, GamepadAxisSource>) {
                // Treat axis as digital with a threshold
                if (s.axis >= 0 && s.axis < SDL_GAMEPAD_AXIS_COUNT)
                    return std::abs(deviceState_.gamepadAxes[s.axis]) > 0.5f;
                return false;
            } else {
                return false;
            }
        },
        source);
}

float InputSystem::readKeyPair(const KeyPairSource& kp) const {
    float value = 0.0f;
    if (deviceState_.keysDown.count(kp.negative))
        value -= 1.0f;
    if (deviceState_.keysDown.count(kp.positive))
        value += 1.0f;
    return value;
}

} // namespace fabric
