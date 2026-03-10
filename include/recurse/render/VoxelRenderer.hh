#pragma once

#include "fabric/core/Spatial.hh"
#include "fabric/render/BgfxHandle.hh"
#include "fabric/world/ChunkedGrid.hh"
#include "recurse/world/SmoothVoxelVertex.hh"
#include <array>
#include <bgfx/bgfx.h>
#include <cstdint>
#include <vector>

namespace recurse {

// Engine types imported from fabric:: namespace
using fabric::ChunkedGrid;
using fabric::K_CHUNK_SIZE;
namespace Space = fabric::Space;
using fabric::Vector3;

/// GPU-side chunk mesh (vertex/index buffer handles + palette).
/// Formerly defined in VoxelMesher.hh; kept here for VoxelRenderer API.
struct ChunkMesh {
    bgfx::VertexBufferHandle vbh = BGFX_INVALID_HANDLE;
    bgfx::IndexBufferHandle ibh = BGFX_INVALID_HANDLE;
    uint32_t indexCount = 0;
    std::vector<std::array<float, 4>> palette;
    bool valid = false;
};

// Per-chunk info for batch rendering.
struct ChunkRenderInfo {
    const ChunkMesh* mesh;
    float offsetX;
    float offsetY;
    float offsetZ;
    uint64_t sortKey = 0; // For deterministic ordering (packed chunk coords)
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

    // Render a single chunk mesh at the given camera-relative offset.
    void render(bgfx::ViewId view, const ChunkMesh& mesh, float offsetX, float offsetY, float offsetZ);

    // Batch-render multiple chunks. Groups by palette when MDI is supported
    // to reduce uniform uploads. Falls back to per-chunk render() otherwise.
    void renderBatch(bgfx::ViewId view, const ChunkRenderInfo* chunks, uint32_t count);

    // Set the directional light direction (normalized, pointing toward light).
    void setLightDirection(const Vector3<float, Space::World>& dir);

    // Set the camera world position used by smooth shading view-dependent terms.
    void setViewPosition(double x, double y, double z);

    // Check if the shader program handle is valid
    bool isValid() const;

    // Whether MDI (multi-draw indirect) is supported on this backend.
    bool mdiSupported() const;

  private:
    void initProgram();
    void renderIndirect(bgfx::ViewId view, const ChunkRenderInfo* chunks, uint32_t count);

    fabric::BgfxHandle<bgfx::ProgramHandle> program_;
    fabric::BgfxHandle<bgfx::UniformHandle> uniformPalette_;
    fabric::BgfxHandle<bgfx::UniformHandle> uniformLightDir_;
    fabric::BgfxHandle<bgfx::UniformHandle> uniformViewPos_;
    fabric::BgfxHandle<bgfx::UniformHandle> uniformLitColor_;
    fabric::BgfxHandle<bgfx::UniformHandle> uniformShadowColor_;
    fabric::BgfxHandle<bgfx::UniformHandle> uniformRimParams_;
    fabric::BgfxHandle<bgfx::UniformHandle> uniformOceanParams_;
    fabric::BgfxHandle<bgfx::IndirectBufferHandle> indirectBuffer_;
    bool initialized_ = false;
    bool mdiSupported_ = false;
    float lightDir_[4] = {0.0f, -1.0f, 0.0f, 0.0f};
    float viewPos_[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    float litColor_[4] = {0.95f, 0.85f, 0.55f, 1.0f};
    float shadowColor_[4] = {0.45f, 0.35f, 0.55f, 1.0f};
    float rimParams_[4] = {3.0f, 0.6f, 0.0f, 0.0f};
    float oceanParams_[4] = {16.0f, 0.8f, 0.0f, 0.0f};

    static constexpr uint32_t K_MAX_INDIRECT_DRAWS = 1024;
};

} // namespace recurse
