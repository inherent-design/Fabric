#pragma once

#include "fabric/ui/RmlPanel.hh"
#include <cstddef>

namespace fabric {

struct ChunkDebugData {
    size_t activeChunks = 0;
    int chunksRendered = 0;
    int chunksMeshed = 0;
    size_t gpuMeshCount = 0;
    size_t dirtyChunksPending = 0;
    size_t vertexCount = 0;
    size_t indexCount = 0;
    float vertexBufferMB = 0.0f;
    float indexBufferMB = 0.0f;
};

class ChunkDebugPanel : public RmlPanel {
  public:
    ChunkDebugPanel() = default;
    ~ChunkDebugPanel() override = default;

    void init(Rml::Context* context);
    void update(const ChunkDebugData& data);

  private:
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
};

} // namespace fabric
