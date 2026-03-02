#pragma once

#include "fabric/core/InputContext.hh"
#include "fabric/core/SystemBase.hh"
#include <array>
#include <functional>
#include <memory>
#include <SDL3/SDL.h>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Rml {
class Context;
}

namespace fabric {

class EventDispatcher;
class InputRecorder;

/// Identifies a connected input device
struct DeviceInfo {
    SDL_JoystickID id = 0;
    std::string name;
    bool isGamepad = false;
    SDL_Gamepad* gamepad = nullptr;
};

/// Raw per-frame device state. Updated by processEvent(), read by context
/// evaluation. This is the "truth" layer between SDL events and bindings.
struct DeviceState {
    // Keyboard
    std::unordered_set<SDL_Keycode> keysDown;

    // Mouse
    float mouseX = 0, mouseY = 0;
    float mouseDeltaX = 0, mouseDeltaY = 0;
    float scrollDeltaX = 0, scrollDeltaY = 0;
    std::array<bool, 5> mouseButtons = {};

    // Gamepad (first connected for MVP)
    std::array<bool, SDL_GAMEPAD_BUTTON_COUNT> gamepadButtons = {};
    std::array<float, SDL_GAMEPAD_AXIS_COUNT> gamepadAxes = {};
    bool gamepadConnected = false;
};

/// Callback for device hot-plug events
using DeviceCallback = std::function<void(const DeviceInfo& device)>;

/// Central input coordinator. Owns the context stack, device state, and
/// provides the query API for game code.
///
/// Lifecycle:
///   1. Construct with optional EventDispatcher
///   2. Load contexts from config or build programmatically
///   3. Each frame: beginFrame() -> processEvent() per SDL event -> evaluate()
///   4. Query: isActionActive(), getAxisValue()
///
/// Extends System<InputSystem> for lifecycle integration with SystemRegistry.
/// Also usable standalone without registration.
class InputSystem : public System<InputSystem> {
  public:
    InputSystem();
    explicit InputSystem(EventDispatcher& dispatcher);

    // --- System lifecycle overrides ---
    void init(AppContext& ctx) override;
    void shutdown() override;
    void update(AppContext& ctx, float dt) override;

    // --- Frame lifecycle ---

    /// Reset per-frame deltas (mouse delta, scroll delta).
    /// Call at the start of each frame before processing events.
    void beginFrame();

    /// Process a single SDL event. Updates raw device state.
    /// Returns true if the event was recognized as input.
    bool processEvent(const SDL_Event& event);

    /// Evaluate all contexts against current device state.
    /// Updates action snapshots and axis values.
    /// Call after all events for this frame have been processed.
    void evaluate();

    // --- Context stack ---

    /// Push a context onto the stack. Contexts are sorted by priority
    /// (highest first) on insertion.
    void pushContext(std::shared_ptr<InputContext> context);

    /// Remove a context by name. Returns true if found and removed.
    bool popContext(const std::string& name);

    /// Remove all contexts
    void clearContexts();

    /// Find a context by name (returns nullptr if not found)
    InputContext* findContext(const std::string& name);
    const InputContext* findContext(const std::string& name) const;

    /// Get the ordered context stack (highest priority first)
    const std::vector<std::shared_ptr<InputContext>>& contexts() const;

    // --- Query API (call after evaluate()) ---

    /// Query digital action state across all active contexts.
    ActionSnapshot actionState(const std::string& actionName) const;

    /// Convenience: true if action is JustPressed or Held
    bool isActionActive(const std::string& actionName) const;

    /// Convenience: true only on the frame the action was first pressed
    bool isActionJustPressed(const std::string& actionName) const;

    /// Convenience: true only on the frame the action was released
    bool isActionJustReleased(const std::string& actionName) const;

    /// Query analog axis value across all active contexts.
    float getAxisValue(const std::string& axisName) const;

    // --- Raw device state ---

    const DeviceState& deviceState() const;

    // --- Device management ---

    void onDeviceConnected(DeviceCallback callback);
    void onDeviceDisconnected(DeviceCallback callback);
    const std::vector<DeviceInfo>& connectedDevices() const;

    /// Trigger rumble on the primary gamepad.
    void rumble(uint16_t lowFreq, uint16_t highFreq, uint32_t durationMs);

    // --- RmlUI integration ---

    /// Forward an SDL event to an RmlUI context. Returns true if consumed.
    static bool routeToRmlUI(const SDL_Event& event, Rml::Context* rmlContext);

    // --- Recording ---

    void setRecorder(InputRecorder* recorder);

    // --- EventDispatcher integration ---

    void setDispatcher(EventDispatcher* dispatcher);

  private:
    EventDispatcher* dispatcher_ = nullptr;
    InputRecorder* recorder_ = nullptr;
    DeviceState deviceState_;
    std::vector<std::shared_ptr<InputContext>> contexts_;
    std::vector<DeviceInfo> connectedDevices_;
    std::vector<DeviceCallback> onConnected_;
    std::vector<DeviceCallback> onDisconnected_;

    // Per-frame evaluated state
    std::unordered_map<std::string, ActionSnapshot> actionSnapshots_;
    std::unordered_map<std::string, float> axisValues_;

    // Previous frame's active actions (for JustPressed/JustReleased detection)
    std::unordered_set<std::string> prevActiveActions_;

    // Tracks oneShot actions that have fired and are waiting for source release
    std::unordered_set<std::string> oneShotFired_;

    void handleGamepadAdded(SDL_JoystickID id);
    void handleGamepadRemoved(SDL_JoystickID id);

    /// Evaluate a single action binding against current device state.
    bool evaluateAction(const ActionBinding& binding) const;

    /// Evaluate a single axis binding against current device state.
    float evaluateAxis(const AxisBinding& binding) const;

    /// Read raw analog value from a single InputSource
    float readSourceAnalog(const InputSource& source) const;

    /// Read digital state from a single InputSource
    bool readSourceDigital(const InputSource& source) const;

    /// Read digital state from a key pair (for axis evaluation)
    float readKeyPair(const KeyPairSource& kp) const;
};

} // namespace fabric
