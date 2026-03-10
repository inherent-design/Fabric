#include "fabric/ui/ConcurrencyPanel.hh"

#include "fabric/core/Log.hh"
#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/DataModelHandle.h>

namespace fabric {

void ConcurrencyPanel::init(Rml::Context* context) {
    if (!context) {
        FABRIC_LOG_ERROR("ConcurrencyPanel::init called with null context");
        return;
    }

    Rml::DataModelConstructor constructor = context->CreateDataModel("concurrency");
    if (!constructor) {
        FABRIC_LOG_ERROR("ConcurrencyPanel: failed to create data model");
        return;
    }

    constructor.Bind("active_workers", &activeWorkers_);
    constructor.Bind("queued_jobs", &queuedJobs_);

    modelHandle_ = constructor.GetModelHandle();

    initBase(context, "concurrency", "assets/ui/concurrency.rml");
    FABRIC_LOG_INFO("ConcurrencyPanel document loaded");
}

void ConcurrencyPanel::update(const ConcurrencyData& data) {
    if (!initialized_)
        return;

    activeWorkers_ = data.activeWorkers;
    queuedJobs_ = data.queuedJobs;

    if (modelHandle_) {
        modelHandle_.DirtyAllVariables();
    }
}

} // namespace fabric
