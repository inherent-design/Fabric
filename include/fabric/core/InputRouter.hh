#pragma once

#include "fabric/core/InputManager.hh"
#include <RmlUi/Core/Input.h>
#include <SDL3/SDL.h>

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

    // SDL-to-RmlUI key mapping (public for testing)
    static Rml::Input::KeyIdentifier sdlKeyToRmlKey(SDL_Keycode key);
    static int sdlModToRmlMod(SDL_Keymod mod);

  private:
    InputManager& inputMgr_;
    InputMode mode_ = InputMode::GameOnly;

    bool forwardToRmlUI(const SDL_Event& event, Rml::Context* ctx);
    bool forwardToGame(const SDL_Event& event);

    static int sdlMouseButtonToRml(Uint8 button);
    static bool isBodyElement(Rml::Element* element);
};

} // namespace fabric
