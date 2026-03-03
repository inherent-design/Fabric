#pragma once

namespace Rml {
class Context;
class ElementDocument;
} // namespace Rml

namespace fabric {

/// Static hotkey reference panel displayed in the bottom-right corner.
/// Contains a hardcoded list of keybinds. No data model needed since
/// the content is static HTML. Toggles alongside the debug HUD (F3).
// TODO: Replace hardcoded list with runtime enumeration from InputManager
// once a public bindings accessor is added.
class HotkeyPanel {
  public:
    HotkeyPanel() = default;
    ~HotkeyPanel() = default;

    void init(Rml::Context* context);
    void toggle();
    void shutdown();
    bool isVisible() const;

  private:
    Rml::Context* context_ = nullptr;
    Rml::ElementDocument* document_ = nullptr;
    bool visible_ = false;
    bool initialized_ = false;
};

} // namespace fabric
