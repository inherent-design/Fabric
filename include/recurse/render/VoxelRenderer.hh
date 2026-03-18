#pragma once

#include "fabric/core/Spatial.hh"
#include "fabric/render/BgfxHandle.hh"
#include "fabric/world/ChunkedGrid.hh"
#include "recurse/simulation/VoxelConstants.hh"
#include "recurse/world/SmoothVoxelVertex.hh"
#include "recurse/world/VoxelVertex.hh"
#include <array>
#include <bgfx/bgfx.h>
#include <cstdint>
#include <vector>

namespace recurse {

// Engine types imported from fabric:: namespace
using fabric::ChunkedGrid;
using recurse::simulation::K_CHUNK_SIZE;
namespace Space = fabric::Space;
using fabric::Vector3;

/// GPU-side chunk mesh (vertex/index buffer handles + palette).
/// Defaults to the voxel-first production format; smooth remains available for
/// experimental fallback meshes. Move-only; BgfxHandle RAII destroys GPU
/// buffers automatically.
struct ChunkMesh {
    enum class VertexFormat : uint8_t {
        Smooth,
        Voxel,
    };

    static constexpr uint32_t vertexStrideForFormat(VertexFormat format) {
        switch (format) {
            case VertexFormat::Smooth:
                return sizeof(SmoothVoxelVertex);
            case VertexFormat::Voxel:
                return sizeof(VoxelVertex);
        }
        return sizeof(VoxelVertex);
    }

    fabric::BgfxHandle<bgfx::VertexBufferHandle> vbh;
    fabric::BgfxHandle<bgfx::IndexBufferHandle> ibh;
    uint32_t indexCount = 0;
    std::vector<std::array<float, 4>> palette;
    VertexFormat vertexFormat = VertexFormat::Voxel;
    uint32_t vertexStrideBytes = vertexStrideForFormat(VertexFormat::Voxel);
    float modelScale = 1.0f;
    bool valid = false;

    ChunkMesh() = default;
    ~ChunkMesh() = default;
    ChunkMesh(const ChunkMesh&) = delete;
    ChunkMesh& operator=(const ChunkMesh&) = delete;
    ChunkMesh(ChunkMesh&&) noexcept = default;
    ChunkMesh& operator=(ChunkMesh&&) noexcept = default;
};

// Per-chunk info for batch rendering.
struct ChunkRenderInfo {
    const ChunkMesh* mesh;
    float offsetX;
    float offsetY;
    float offsetZ;
    uint64_t sortKey = 0; // For deterministic ordering (packed chunk coords)
};

// Renders chunk meshes using the voxel-first default path, with optional smooth
// fallback routing selected by ChunkMesh::vertexFormat. Manages the bgfx shader
// programs, palette uniform, and light direction uniform.
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

    // Toggle explicit chunk wireframe rendering for the voxel path.
    void setWireframeEnabled(bool enabled);
    bool isWireframeEnabled() const;

    // Check if the shader program handle is valid
    bool isValid() const;

    // Whether MDI (multi-draw indirect) is supported on this backend.
    bool mdiSupported() const;

  private:
    void initPrograms();
    void renderIndirect(bgfx::ViewId view, const ChunkRenderInfo* chunks, uint32_t count);
    uint64_t renderState() const;
    bgfx::ProgramHandle programForFormat(ChunkMesh::VertexFormat format) const;

    fabric::BgfxHandle<bgfx::ProgramHandle> smoothProgram_;
    fabric::BgfxHandle<bgfx::ProgramHandle> voxelProgram_;
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
    bool wireframeEnabled_ = false;
    float lightDir_[4] = {0.0f, -1.0f, 0.0f, 0.0f};
    float viewPos_[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    float litColor_[4] = {0.95f, 0.85f, 0.55f, 1.0f};
    float shadowColor_[4] = {0.45f, 0.35f, 0.55f, 1.0f};
    float rimParams_[4] = {3.0f, 0.6f, 0.0f, 0.0f};
    float oceanParams_[4] = {16.0f, 0.8f, 0.0f, 0.0f};

    static constexpr uint32_t K_MAX_INDIRECT_DRAWS = 1024;
};

} // namespace recurse
