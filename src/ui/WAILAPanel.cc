#include "fabric/ui/WAILAPanel.hh"

#include "fabric/core/Log.hh"
#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/DataModelHandle.h>

#include <cstdio>

namespace fabric {

void WAILAPanel::init(Rml::Context* context) {
    if (!context) {
        FABRIC_LOG_ERROR("WAILAPanel::init called with null context");
        return;
    }

    Rml::DataModelConstructor constructor = context->CreateDataModel("waila");
    if (!constructor) {
        FABRIC_LOG_ERROR("WAILAPanel: failed to create data model");
        return;
    }

    constructor.Bind("has_hit", &hasHit_);
    constructor.Bind("display_text", &displayText_);
    constructor.Bind("voxel_x", &voxelX_);
    constructor.Bind("voxel_y", &voxelY_);
    constructor.Bind("voxel_z", &voxelZ_);
    constructor.Bind("chunk_x", &chunkX_);
    constructor.Bind("chunk_y", &chunkY_);
    constructor.Bind("chunk_z", &chunkZ_);
    constructor.Bind("normal_str", &normalStr_);
    constructor.Bind("distance", &distance_);
    constructor.Bind("density", &density_);
    constructor.Bind("essence_str", &essenceStr_);

    modelHandle_ = constructor.GetModelHandle();

    initBase(context, "waila", "assets/ui/waila.rml");
    FABRIC_LOG_INFO("WAILAPanel document loaded");
}

void WAILAPanel::update(const WAILAData& data) {
    if (!initialized_)
        return;

    hasHit_ = data.hasHit;

    if (data.hasHit) {
        displayText_ = "";
        voxelX_ = data.voxelX;
        voxelY_ = data.voxelY;
        voxelZ_ = data.voxelZ;
        chunkX_ = data.chunkX;
        chunkY_ = data.chunkY;
        chunkZ_ = data.chunkZ;
        distance_ = data.distance;
        density_ = data.density;

        char normalBuf[16];
        std::snprintf(normalBuf, sizeof(normalBuf), "%d, %d, %d", data.normalX, data.normalY, data.normalZ);
        normalStr_ = normalBuf;

        auto clamp = [](float v) -> int {
            return static_cast<int>(v * 255.0f + 0.5f) & 0xFF;
        };
        char essBuf[16];
        std::snprintf(essBuf, sizeof(essBuf), "#%02X%02X%02X", clamp(data.essenceR), clamp(data.essenceG),
                      clamp(data.essenceB));
        essenceStr_ = essBuf;
    } else {
        displayText_ = "Sky";
    }

    if (modelHandle_) {
        modelHandle_.DirtyAllVariables();
    }
}

void WAILAPanel::setMode(AppMode mode) {
    if (!initialized_)
        return;

    currentMode_ = mode;

    bool shouldShow = (mode == AppMode::Game || mode == AppMode::Paused);

    if (document_) {
        if (shouldShow && !visible_) {
            document_->Show();
            visible_ = true;
        } else if (!shouldShow && visible_) {
            document_->Hide();
            visible_ = false;
        }
    }
}

} // namespace fabric
