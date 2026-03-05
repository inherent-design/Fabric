#pragma once

#include <cstddef>
#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/Types.h>

namespace Rml {
class Context;
class ElementDocument;
} // namespace Rml

namespace fabric {

/// Chunk rendering/meshing statistics collected per-frame.
struct ChunkDebugData {
    size_t activeChunks = 0;       // Total chunks with GPU meshes
    int chunksRendered = 0;        // Chunks drawn this frame
    int chunksMeshed = 0;          // Chunks meshed this frame
    size_t gpuMeshCount = 0;       // GPU mesh buffers allocated
    size_t dirtyChunksPending = 0; // Chunks awaiting mesh update
    size_t vertexCount = 0;        // Total vertices across all meshes
    size_t indexCount = 0;         // Total indices across all meshes
    float vertexBufferMB = 0.0f;   // Vertex buffer size in MB
    float indexBufferMB = 0.0f;    // Index buffer size in MB
};

/// Debug panel for chunk render/mesh statistics.
/// Displays active chunk count, meshing throughput, GPU memory usage.
/// Toggle with F5 key.
class ChunkDebugPanel {
  public:
    ChunkDebugPanel() = default;
    ~ChunkDebugPanel() = default;

    void init(Rml::Context* context);
    void update(const ChunkDebugData& data);
    void toggle();
    void shutdown();
    bool isVisible() const;

  private:
    Rml::Context* context_ = nullptr;
    Rml::ElementDocument* document_ = nullptr;
    bool visible_ = false;
    bool initialized_ = false;

    // Bound variables for the "chunk_debug" data model
    int activeChunks_ = 0;
    int chunksRendered_ = 0;
    int chunksMeshed_ = 0;
    int gpuMeshCount_ = 0;
    int dirtyChunksPending_ = 0;
    int vertexCount_ = 0;
    int indexCount_ = 0;
    float vertexBufferMB_ = 0.0f;
    float indexBufferMB_ = 0.0f;
    float totalBufferMB_ = 0.0f;

    Rml::DataModelHandle modelHandle_;
};

} // namespace fabric
