#include "fabric/ui/ChunkDebugPanel.hh"

#include "fabric/core/Log.hh"
#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/ElementDocument.h>

namespace fabric {

void ChunkDebugPanel::init(Rml::Context* context) {
    if (!context) {
        FABRIC_LOG_ERROR("ChunkDebugPanel::init called with null context");
        return;
    }

    context_ = context;

    Rml::DataModelConstructor constructor = context_->CreateDataModel("chunk_debug");
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

    modelHandle_ = constructor.GetModelHandle();

    document_ = context_->LoadDocument("assets/ui/chunk_debug.rml");
    if (document_) {
        document_->Hide();
        FABRIC_LOG_INFO("ChunkDebugPanel document loaded, id={}", document_->GetId());
    } else {
        FABRIC_LOG_ERROR("ChunkDebugPanel: failed to load chunk_debug.rml");
    }

    initialized_ = true;
}

void ChunkDebugPanel::update(const ChunkDebugData& data) {
    if (!initialized_) {
        return;
    }

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

    if (modelHandle_) {
        modelHandle_.DirtyAllVariables();
    }
}

void ChunkDebugPanel::toggle() {
    visible_ = !visible_;
    FABRIC_LOG_INFO("ChunkDebugPanel toggle: visible_={}, initialized_={}", visible_, initialized_);
    if (!initialized_)
        return;
    if (document_) {
        if (visible_) {
            document_->Show();
            FABRIC_LOG_INFO("ChunkDebugPanel: showing document");
        } else {
            document_->Hide();
            FABRIC_LOG_INFO("ChunkDebugPanel: hiding document");
        }
    }
}

void ChunkDebugPanel::shutdown() {
    initialized_ = false;
    if (document_) {
        document_->Close();
        document_ = nullptr;
    }
    if (context_ && modelHandle_) {
        context_->RemoveDataModel("chunk_debug");
    }
    modelHandle_ = {};
    visible_ = false;
    context_ = nullptr;
}

bool ChunkDebugPanel::isVisible() const {
    return visible_;
}

} // namespace fabric
