#include "fabric/ui/HotkeyPanel.hh"

#include "fabric/core/Log.hh"
#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/ElementDocument.h>

namespace fabric {

void HotkeyPanel::init(Rml::Context* context) {
    if (!context) {
        FABRIC_LOG_ERROR("HotkeyPanel::init called with null context");
        return;
    }

    context_ = context;

    document_ = context_->LoadDocument("assets/ui/hotkey_ref.rml");
    if (document_) {
        document_->Hide();
        FABRIC_LOG_INFO("HotkeyPanel document loaded");
    } else {
        FABRIC_LOG_WARN("HotkeyPanel: failed to load hotkey_ref.rml");
    }

    initialized_ = true;
}

void HotkeyPanel::toggle() {
    visible_ = !visible_;
    if (document_) {
        if (visible_) {
            document_->Show();
        } else {
            document_->Hide();
        }
    }
}

void HotkeyPanel::shutdown() {
    if (document_) {
        document_->Close();
        document_ = nullptr;
    }
    initialized_ = false;
    visible_ = false;
    context_ = nullptr;
}

bool HotkeyPanel::isVisible() const {
    return visible_;
}

} // namespace fabric
