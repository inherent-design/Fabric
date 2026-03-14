#include "fabric/ui/ChunkDebugPanel.hh"

#include "fabric/log/Log.hh"
#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/DataModelHandle.h>

namespace fabric {

void ChunkDebugPanel::init(Rml::Context* context) {
    if (!context) {
        FABRIC_LOG_ERROR("ChunkDebugPanel::init called with null context");
        return;
    }

    Rml::DataModelConstructor constructor = context->CreateDataModel("chunk_debug");
    if (!constructor) {
        FABRIC_LOG_ERROR("ChunkDebugPanel: failed to create data model");
        return;
    }

    constructor.Bind("active_chunks", &activeChunks_);
    constructor.Bind("chunks_rendered", &chunksRendered_);
    constructor.Bind("chunks_meshed", &chunksMeshed_);
    constructor.Bind("gpu_mesh_count", &gpuMeshCount_);
    constructor.Bind("dirty_chunks_pending", &dirtyChunksPending_);
    constructor.Bind("vertex_count", &vertexCount_);
    constructor.Bind("index_count", &indexCount_);
    constructor.Bind("vertex_buffer_mb", &vertexBufferMB_);
    constructor.Bind("index_buffer_mb", &indexBufferMB_);
    constructor.Bind("total_buffer_mb", &totalBufferMB_);
    constructor.Bind("meshed_this_frame", &meshedThisFrame_);
    constructor.Bind("empty_chunks_skipped", &emptyChunksSkipped_);
    constructor.Bind("mesh_budget_remaining", &meshBudgetRemaining_);

    modelHandle_ = constructor.GetModelHandle();

    initBase(context, "chunk_debug", "assets/ui/chunk_debug.rml");
    FABRIC_LOG_INFO("ChunkDebugPanel document loaded");
}

void ChunkDebugPanel::update(const ChunkDebugData& data) {
    if (!initialized_)
        return;

    activeChunks_ = static_cast<int>(data.activeChunks);
    chunksRendered_ = data.chunksRendered;
    chunksMeshed_ = data.chunksMeshed;
    gpuMeshCount_ = static_cast<int>(data.gpuMeshCount);
    dirtyChunksPending_ = static_cast<int>(data.dirtyChunksPending);
    vertexCount_ = static_cast<int>(data.vertexCount);
    indexCount_ = static_cast<int>(data.indexCount);
    vertexBufferMB_ = data.vertexBufferMB;
    indexBufferMB_ = data.indexBufferMB;
    totalBufferMB_ = data.vertexBufferMB + data.indexBufferMB;
    meshedThisFrame_ = data.meshedThisFrame;
    emptyChunksSkipped_ = data.emptyChunksSkipped;
    meshBudgetRemaining_ = data.meshBudgetRemaining;

    if (modelHandle_) {
        modelHandle_.DirtyAllVariables();
    }
}

} // namespace fabric
