#pragma once

#include "fabric/core/InputManager.hh"
#include <functional>
#include <RmlUi/Core/Input.h>
#include <SDL3/SDL.h>
#include <unordered_map>

namespace Rml {
class Context;
class Element;
} // namespace Rml

namespace fabric {

enum class InputMode {
    GameOnly,
    UIOnly,
    GameAndUI
};

class InputRouter {
  public:
    explicit InputRouter(InputManager& inputMgr);

    void setMode(InputMode mode);
    InputMode mode() const;

    // Route an SDL event. Returns true if consumed.
    // When Rml::Context is null, UI forwarding is skipped.
    bool routeEvent(const SDL_Event& event, Rml::Context* rmlContext);

    // Call at frame start (resets per-frame state)
    void beginFrame();

    // Toggle between GameOnly and UIOnly
    void toggleUIMode();

    // Console toggle callback (backtick key)
    void setConsoleToggleCallback(std::function<void()> cb) { consoleToggleCallback_ = std::move(cb); }

    // Escape callback (overrides default toggleUIMode behavior)
    void setEscapeCallback(std::function<void()> cb) { escapeCallback_ = std::move(cb); }

    /// Set a hook that receives every SDL event before routing.
    /// The hook is called with the raw SDL_Event for serialization/recording.
    /// Pass nullptr to detach.
    void setEventCaptureHook(std::function<void(const SDL_Event&)> hook);

    /// Set a hook called once per frame from beginFrame().
    /// Used for frame boundary tracking in recording/playback.
    /// Pass nullptr to detach.
    void setFrameHook(std::function<void()> hook);

    /// Register a callback that fires on non-repeat key-down events.
    /// Callbacks are suppressed in UIOnly mode. Replaces any previous
    /// callback for the same key.
    void registerKeyCallback(SDL_Keycode key, std::function<void()> cb);

    /// Remove a previously registered key callback.
    void unregisterKeyCallback(SDL_Keycode key);

    // SDL-to-RmlUI key mapping (public for testing)
    static Rml::Input::KeyIdentifier sdlKeyToRmlKey(SDL_Keycode key);
    static int sdlModToRmlMod(SDL_Keymod mod);

  private:
    InputManager& inputMgr_;
    InputMode mode_ = InputMode::GameOnly;
    std::function<void(const SDL_Event&)> eventCaptureHook_;
    std::function<void()> frameHook_;
    std::function<void()> consoleToggleCallback_;
    std::function<void()> escapeCallback_;
    std::unordered_map<SDL_Keycode, std::function<void()>> keyCallbacks_;

    bool forwardToRmlUI(const SDL_Event& event, Rml::Context* ctx);
    bool forwardToGame(const SDL_Event& event);

    static int sdlMouseButtonToRml(Uint8 button);
    static bool isBodyElement(Rml::Element* element);
};

} // namespace fabric
