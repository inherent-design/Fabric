#pragma once

#include "fabric/core/AppModeManager.hh"
#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/Types.h>
#include <vector>

namespace Rml {
class Context;
class ElementDocument;
} // namespace Rml

namespace fabric {

struct HotkeyEntry {
    Rml::String category;
    Rml::String key;
    Rml::String action;
};

/// Context-aware hotkey reference panel displayed in the bottom-right corner.
/// Updates displayed hotkeys based on current AppMode (Game, Paused, Console, etc.).
/// Always visible during gameplay.
class HotkeyPanel {
  public:
    HotkeyPanel() = default;
    ~HotkeyPanel() = default;

    void init(Rml::Context* context);
    void setMode(AppMode mode);
    void shutdown();
    bool isVisible() const;

  private:
    void rebuildHotkeys();
    void addHotkey(const char* category, const char* key, const char* action);

    Rml::Context* context_ = nullptr;
    Rml::ElementDocument* document_ = nullptr;
    bool initialized_ = false;
    bool visible_ = false;
    AppMode currentMode_ = AppMode::Menu; // Initialize to Menu so first setMode() triggers rebuild

    // Data model bindings
    Rml::String modeName_;
    std::vector<HotkeyEntry> hotkeys_;
    Rml::DataModelHandle modelHandle_;
};

} // namespace fabric
