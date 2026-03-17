#include "recurse/ui/LODStatsPanel.hh"

#include "fabric/log/Log.hh"
#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/DataModelHandle.h>

namespace recurse {

void LODStatsPanel::init(Rml::Context* context) {
    if (!context) {
        FABRIC_LOG_ERROR("LODStatsPanel::init called with null context");
        return;
    }

    Rml::DataModelConstructor constructor = context->CreateDataModel("lod_stats");
    if (!constructor) {
        FABRIC_LOG_ERROR("LODStatsPanel: failed to create data model");
        return;
    }

    constructor.Bind("pending_sections", &pendingSections_);
    constructor.Bind("gpu_resident_sections", &gpuResidentSections_);
    constructor.Bind("visible_sections", &visibleSections_);
    constructor.Bind("estimated_gpu_mb", &estimatedGpuMB_);

    modelHandle_ = constructor.GetModelHandle();

    initBase(context, "lod_stats", "assets/ui/lod_stats.rml");
    FABRIC_LOG_INFO("LODStatsPanel document loaded");
}

void LODStatsPanel::update(const LODStatsData& data) {
    if (!initialized_)
        return;

    pendingSections_ = data.pendingSections;
    gpuResidentSections_ = data.gpuResidentSections;
    visibleSections_ = data.visibleSections;
    estimatedGpuMB_ = data.estimatedGpuMB;

    if (modelHandle_) {
        modelHandle_.DirtyAllVariables();
    }
}

} // namespace recurse
