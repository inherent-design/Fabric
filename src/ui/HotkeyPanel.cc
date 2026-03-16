#include "fabric/ui/HotkeyPanel.hh"

#include "fabric/log/Log.hh"
#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>

namespace fabric {

void HotkeyPanel::init(Rml::Context* context) {
    if (!context) {
        FABRIC_LOG_ERROR("HotkeyPanel::init called with null context");
        return;
    }

    Rml::DataModelConstructor constructor = context->CreateDataModel("hotkeys");
    if (constructor) {
        constructor.Bind("mode_name", &modeName_);
        modelHandle_ = constructor.GetModelHandle();
    }

    initBase(context, "hotkeys", "assets/ui/hotkey_ref.rml");

    // HotkeyPanel starts visible (visible_ = true by default via setMode)
    visible_ = true;
    if (document_) {
        document_->Show();
    }

    FABRIC_LOG_INFO("HotkeyPanel::init complete");
}

void HotkeyPanel::setMode(AppMode mode) {
    if (!initialized_) {
        FABRIC_LOG_WARN("HotkeyPanel::setMode called before init");
        return;
    }

    bool modeChanged = (mode != currentMode_);
    currentMode_ = mode;

    bool shouldShow = (mode != AppMode::Menu);

    if (document_) {
        if (shouldShow && !visible_) {
            document_->Show();
            visible_ = true;
        } else if (!shouldShow && visible_) {
            document_->Hide();
            visible_ = false;
        }
    }

    if (modeChanged) {
        rebuildHotkeys();
    }
}

void HotkeyPanel::rebuildHotkeys() {
    if (!document_)
        return;

    switch (currentMode_) {
        case AppMode::Game:
            modeName_ = "Game";
            break;
        case AppMode::Paused:
            modeName_ = "Paused";
            break;
        case AppMode::Console:
            modeName_ = "Console";
            break;
        case AppMode::Editor:
            modeName_ = "Editor";
            break;
        case AppMode::Menu:
            modeName_ = "Menu";
            break;
        default:
            modeName_ = "Unknown";
            break;
    }

    if (modelHandle_) {
        modelHandle_.DirtyVariable("mode_name");
    }

    auto* panel = document_->GetElementById("hotkey-panel");
    if (!panel)
        return;

    while (panel->GetNumChildren() > 1) {
        panel->RemoveChild(panel->GetLastChild());
    }

    hotkeys_.clear();

    switch (currentMode_) {
        case AppMode::Game:
            addHotkey("Movement", "W A S D", "Move");
            addHotkey("Movement", "Space", "Up / Jump");
            addHotkey("Movement", "LShift", "Down / Crouch");
            addHotkey("Movement", "F", "Toggle Fly");
            addHotkey("Camera", "V", "1st/3rd Person");
            addHotkey("Camera", "Alt+C", "Panini/Equirect");
            addHotkey("Interaction", "LMB", "Destroy Voxel");
            addHotkey("Interaction", "RMB", "Place Voxel");
            addHotkey("Time", "P", "Pause Time");
            addHotkey("Time", "+ / -", "Time Scale");
            addHotkey("Debug", "F1", "Chunk States");
            addHotkey("Debug", "F2", "LOD Stats");
            addHotkey("Debug", "F3", "Debug HUD");
            addHotkey("Debug", "F4", "Wireframe");
            addHotkey("Debug", "F5", "Concurrency");
            addHotkey("Debug", "F6", "BVH Overlay");
            addHotkey("Debug", "F10", "Collision Debug");
            addHotkey("Debug", "F12", "Chunk Debug");
            addHotkey("Panels", "`", "Console");
            addHotkey("Panels", "Esc", "Pause Menu");
            addHotkey("Panels", "F7", "Content Browser");
            addHotkey("Panels", "F8", "Cycle BT NPC");
            addHotkey("Panels", "F11", "BT Debug");
            break;

        case AppMode::Paused:
            addHotkey("Navigation", "Esc", "Resume Game");
            addHotkey("UI", "LMB", "Click Buttons");
            addHotkey("Panels", "`", "Console");
            break;

        case AppMode::Console:
            addHotkey("Console", "Esc", "Close Console");
            addHotkey("Console", "Enter", "Execute Command");
            addHotkey("Console", "Up / Down", "Command History");
            addHotkey("Panels", "`", "Toggle Console");
            break;

        case AppMode::Editor:
            addHotkey("Editor", "Esc", "Close Browser");
            addHotkey("Editor", "LMB", "Select Asset");
            addHotkey("Debug", "F3", "Debug HUD");
            addHotkey("Panels", "F7", "Toggle Browser");
            break;

        case AppMode::Menu:
            addHotkey("UI", "LMB", "Click Buttons");
            addHotkey("UI", "Esc", "Back / Quit");
            break;

        default:
            break;
    }

    Rml::String currentCategory;
    for (const auto& hk : hotkeys_) {
        if (hk.category != currentCategory) {
            currentCategory = hk.category;
            auto* section = panel->AppendChild(document_->CreateElement("div"));
            section->SetClassNames("hk-section");
            section->SetInnerRML(hk.category);
        }

        auto* row = panel->AppendChild(document_->CreateElement("div"));
        row->SetClassNames("hk-row");

        auto* keySpan = row->AppendChild(document_->CreateElement("span"));
        keySpan->SetClassNames("hk-key");
        keySpan->SetInnerRML(hk.key);

        auto* actionSpan = row->AppendChild(document_->CreateElement("span"));
        actionSpan->SetClassNames("hk-action");
        actionSpan->SetInnerRML(hk.action);
    }
}

void HotkeyPanel::addHotkey(const char* category, const char* key, const char* action) {
    hotkeys_.push_back({Rml::String(category), Rml::String(key), Rml::String(action)});
}

} // namespace fabric
