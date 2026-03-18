#include "recurse/ui/DebugHUD.hh"

#include "fabric/log/Log.hh"
#include "fabric/utils/Profiler.hh"
#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/DataModelHandle.h>

namespace recurse {

void DebugHUD::init(Rml::Context* context) {
    if (!context) {
        FABRIC_LOG_ERROR("DebugHUD::init called with null context");
        return;
    }

    Rml::DataModelConstructor constructor = context->CreateDataModel("debug_hud");
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

    // Perf overlay (EF-18)
    constructor.Bind("draw_call_count", &drawCallCount_);
    constructor.Bind("gpu_time_ms", &gpuTimeMs_);
    constructor.Bind("memory_usage_mb", &memoryUsageMB_);
    constructor.Bind("physics_body_count", &physicsBodyCount_);
    constructor.Bind("audio_voice_count", &audioVoiceCount_);
    constructor.Bind("chunk_mesh_queue_size", &chunkMeshQueueSize_);
    constructor.Bind("autosave_state", &autosaveState_);
    constructor.Bind("autosave_next_save", &autosaveNextSave_);
    constructor.Bind("autosave_dirty_chunks", &autosaveDirtyChunks_);
    constructor.Bind("autosave_saving_chunks", &autosaveSavingChunks_);
    constructor.Bind("autosave_queued_chunks", &autosaveQueuedChunks_);

    modelHandle_ = constructor.GetModelHandle();

    initBase(context, "debug_hud", "assets/ui/debug_hud.rml");
    FABRIC_LOG_INFO("DebugHUD document loaded");
}

void DebugHUD::update(const DebugData& data) {
    if (!initialized_)
        return;

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

    // Perf overlay (EF-18)
    drawCallCount_ = data.drawCallCount;
    gpuTimeMs_ = data.gpuTimeMs;
    memoryUsageMB_ = data.memoryUsageMB;
    physicsBodyCount_ = data.physicsBodyCount;
    audioVoiceCount_ = data.audioVoiceCount;
    chunkMeshQueueSize_ = data.chunkMeshQueueSize;
    autosaveState_ = data.autosaveState;
    autosaveNextSave_ = data.autosaveNextSave;
    autosaveDirtyChunks_ = data.autosaveDirtyChunks;
    autosaveSavingChunks_ = data.autosaveSavingChunks;
    autosaveQueuedChunks_ = data.autosaveQueuedChunks;

    FABRIC_PLOT("Draw Calls", static_cast<int64_t>(data.drawCallCount));
    FABRIC_PLOT("GPU Time (ms)", static_cast<double>(data.gpuTimeMs));
    FABRIC_PLOT("Memory (MB)", static_cast<double>(data.memoryUsageMB));
    FABRIC_PLOT("Physics Bodies", static_cast<int64_t>(data.physicsBodyCount));
    FABRIC_PLOT("Audio Voices", static_cast<int64_t>(data.audioVoiceCount));
    FABRIC_PLOT("Mesh Queue", static_cast<int64_t>(data.chunkMeshQueueSize));

    if (modelHandle_) {
        modelHandle_.DirtyAllVariables();
    }
}

} // namespace recurse
