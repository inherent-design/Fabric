#pragma once

#include "fabric/ui/RmlPanel.hh"
#include <cstddef>

namespace recurse {

struct ChunkDebugData {
    size_t activeChunks = 0;
    int chunksRendered = 0;
    size_t gpuMeshCount = 0;
    size_t dirtyChunksPending = 0;
    size_t vertexCount = 0;
    size_t indexCount = 0;
    float vertexBufferMB = 0.0f;
    float indexBufferMB = 0.0f;
    // Per-frame meshing stats
    int meshedThisFrame = 0;
    int emptyChunksSkipped = 0;
    int meshBudgetRemaining = 0;
};

class ChunkDebugPanel : public fabric::RmlPanel {
  public:
    ChunkDebugPanel() = default;
    ~ChunkDebugPanel() override = default;

    void init(Rml::Context* context);
    void update(const ChunkDebugData& data);

  private:
    int activeChunks_ = 0;
    int chunksRendered_ = 0;
    int gpuMeshCount_ = 0;
    int dirtyChunksPending_ = 0;
    int vertexCount_ = 0;
    int indexCount_ = 0;
    float vertexBufferMB_ = 0.0f;
    float indexBufferMB_ = 0.0f;
    float totalBufferMB_ = 0.0f;
    int meshedThisFrame_ = 0;
    int emptyChunksSkipped_ = 0;
    int meshBudgetRemaining_ = 0;
};

} // namespace recurse
