#include "recurse/ui/AutosavePanel.hh"

#include "fabric/log/Log.hh"
#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/DataModelHandle.h>

namespace recurse {

void AutosavePanel::init(Rml::Context* context) {
    if (!context) {
        FABRIC_LOG_ERROR("AutosavePanel::init called with null context");
        return;
    }

    auto constructor = context->CreateDataModel("autosave");
    if (!constructor) {
        FABRIC_LOG_ERROR("AutosavePanel: failed to create data model");
        return;
    }

    constructor.Bind("status_text", &statusText_);
    constructor.Bind("detail_text", &detailText_);
    modelHandle_ = constructor.GetModelHandle();

    initBase(context, "autosave", "assets/ui/autosave.rml");
}

void AutosavePanel::update(const AutosaveIndicatorData& data) {
    if (!initialized_)
        return;

    statusText_ = data.statusText;
    detailText_ = data.detailText;

    if (document_) {
        if (data.visible && !visible_) {
            document_->Show();
            visible_ = true;
        } else if (!data.visible && visible_) {
            document_->Hide();
            visible_ = false;
        }
    }

    if (modelHandle_)
        modelHandle_.DirtyAllVariables();
}

} // namespace recurse
