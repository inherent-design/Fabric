#include "fabric/ui/DebugHUD.hh"

#include "fabric/core/Log.hh"
#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/ElementDocument.h>

namespace fabric {

void DebugHUD::init(Rml::Context* context) {
    if (!context) {
        FABRIC_LOG_ERROR("DebugHUD::init called with null context");
        return;
    }

    context_ = context;

    Rml::DataModelConstructor constructor = context_->CreateDataModel("debug_hud");
    if (!constructor) {
        FABRIC_LOG_ERROR("DebugHUD: failed to create data model");
        return;
    }

    constructor.Bind("fps", &fps_);
    constructor.Bind("frame_time_ms", &frameTimeMs_);
    constructor.Bind("entity_count", &entityCount_);
    constructor.Bind("visible_chunks", &visibleChunks_);
    constructor.Bind("total_chunks", &totalChunks_);
    constructor.Bind("triangle_count", &triangleCount_);
    constructor.Bind("cam_x", &camX_);
    constructor.Bind("cam_y", &camY_);
    constructor.Bind("cam_z", &camZ_);
    constructor.Bind("current_radius", &currentRadius_);
    constructor.Bind("current_state", &currentState_);

    modelHandle_ = constructor.GetModelHandle();

    document_ = context_->LoadDocument("assets/ui/debug_hud.rml");
    if (document_) {
        document_->Hide();
        FABRIC_LOG_INFO("DebugHUD document loaded");
    } else {
        FABRIC_LOG_WARN("DebugHUD: failed to load debug_hud.rml");
    }

    initialized_ = true;
}

void DebugHUD::update(const DebugData& data) {
    if (!initialized_) {
        return;
    }

    fps_ = data.fps;
    frameTimeMs_ = data.frameTimeMs;
    entityCount_ = data.entityCount;
    visibleChunks_ = data.visibleChunks;
    totalChunks_ = data.totalChunks;
    triangleCount_ = data.triangleCount;
    camX_ = data.cameraPosition.x;
    camY_ = data.cameraPosition.y;
    camZ_ = data.cameraPosition.z;
    currentRadius_ = data.currentRadius;
    currentState_ = data.currentState;

    if (modelHandle_) {
        modelHandle_.DirtyAllVariables();
    }
}

void DebugHUD::toggle() {
    visible_ = !visible_;
    if (document_) {
        if (visible_) {
            document_->Show();
        } else {
            document_->Hide();
        }
    }
}

void DebugHUD::shutdown() {
    if (document_) {
        document_->Close();
        document_ = nullptr;
    }
    if (context_ && modelHandle_) {
        context_->RemoveDataModel("debug_hud");
    }
    modelHandle_ = {};
    initialized_ = false;
    visible_ = false;
    context_ = nullptr;
}

bool DebugHUD::isVisible() const {
    return visible_;
}

} // namespace fabric
