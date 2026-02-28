#pragma once

#include "fabric/core/Spatial.hh"
#include "fabric/core/VoxelMesher.hh"
#include <bgfx/bgfx.h>
#include <cstdint>

namespace fabric {

// Per-chunk info for batch rendering.
struct ChunkRenderInfo {
    const ChunkMesh* mesh;
    int chunkX;
    int chunkY;
    int chunkZ;
};

// Renders voxel chunks using the voxel shader program.
// Manages the bgfx shader program, palette uniform, and light direction uniform.
// Requires bgfx to be initialized before first render call; shader source is
// embedded at compile time via the bgfx shader compiler (offline).
//
// When BGFX_CAPS_DRAW_INDIRECT is available, renderBatch() groups chunks by
// palette to minimize uniform uploads via selective discard flags. The indirect
// buffer is allocated for future VertexPool-backed MDI submission.
class VoxelRenderer {
  public:
    VoxelRenderer();
    ~VoxelRenderer();

    void shutdown();

    VoxelRenderer(const VoxelRenderer&) = delete;
    VoxelRenderer& operator=(const VoxelRenderer&) = delete;

    // Render a single chunk mesh at the given chunk coordinates.
    // The chunk position is converted to world-space transform internally.
    void render(bgfx::ViewId view, const ChunkMesh& mesh, int chunkX, int chunkY, int chunkZ);

    // Batch-render multiple chunks. Groups by palette when MDI is supported
    // to reduce uniform uploads. Falls back to per-chunk render() otherwise.
    void renderBatch(bgfx::ViewId view, const ChunkRenderInfo* chunks, uint32_t count);

    // Set the directional light direction (normalized, pointing toward light).
    void setLightDirection(const Vector3<float, Space::World>& dir);

    // Check if the shader program handle is valid
    bool isValid() const;

    // Whether MDI (multi-draw indirect) is supported on this backend.
    bool mdiSupported() const;

  private:
    void initProgram();
    void renderIndirect(bgfx::ViewId view, const ChunkRenderInfo* chunks, uint32_t count);

    bgfx::ProgramHandle program_;
    bgfx::UniformHandle uniformPalette_;
    bgfx::UniformHandle uniformLightDir_;
    bgfx::IndirectBufferHandle indirectBuffer_;
    bool initialized_ = false;
    bool mdiSupported_ = false;

    static constexpr uint32_t kMaxIndirectDraws = 1024;
};

} // namespace fabric
