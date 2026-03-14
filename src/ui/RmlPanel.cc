#include "fabric/ui/RmlPanel.hh"

#include "fabric/log/Log.hh"
#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/ElementDocument.h>

namespace fabric {

bool RmlPanel::initBase(Rml::Context* context, const char* modelName, const char* rmlPath) {
    if (!context) {
        FABRIC_LOG_ERROR("RmlPanel::initBase called with null context (model={})", modelName);
        return false;
    }

    context_ = context;
    modelName_ = modelName;

    document_ = context_->LoadDocument(rmlPath);
    if (document_) {
        document_->Hide();
    } else {
        FABRIC_LOG_WARN("RmlPanel: failed to load {}", rmlPath);
    }

    initialized_ = true;
    return true;
}

void RmlPanel::toggle() {
    visible_ = !visible_;
    if (!initialized_)
        return;
    if (document_) {
        if (visible_) {
            document_->Show();
        } else {
            document_->Hide();
        }
    }
}

void RmlPanel::shutdown() {
    onShutdown();

    initialized_ = false;
    if (document_) {
        document_->Close();
        document_ = nullptr;
    }
    if (context_ && modelHandle_) {
        context_->RemoveDataModel(modelName_);
    }
    modelHandle_ = {};
    visible_ = false;
    context_ = nullptr;
}

bool RmlPanel::isVisible() const {
    return visible_;
}

} // namespace fabric
