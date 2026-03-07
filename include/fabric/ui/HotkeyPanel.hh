#pragma once

#include "fabric/core/AppModeManager.hh"
#include "fabric/ui/RmlPanel.hh"
#include <RmlUi/Core/Types.h>
#include <vector>

namespace fabric {

struct HotkeyEntry {
    Rml::String category;
    Rml::String key;
    Rml::String action;
};

/// Context-aware hotkey reference panel displayed in the bottom-right corner.
/// Updates displayed hotkeys based on current AppMode (Game, Paused, Console, etc.).
class HotkeyPanel : public RmlPanel {
  public:
    HotkeyPanel() = default;
    ~HotkeyPanel() override = default;

    void init(Rml::Context* context);
    void setMode(AppMode mode);

  private:
    void rebuildHotkeys();
    void addHotkey(const char* category, const char* key, const char* action);

    AppMode currentMode_ = AppMode::Menu;

    Rml::String modeName_;
    std::vector<HotkeyEntry> hotkeys_;
};

} // namespace fabric
