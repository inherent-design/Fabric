#pragma once

#include "fabric/core/ChunkedGrid.hh"
#include "fabric/core/FieldLayer.hh"
#include "fabric/core/Spatial.hh"
#include "fabric/core/VoxelVertex.hh"
#include <array>
#include <bgfx/bgfx.h>
#include <cstdint>
#include <vector>

namespace fabric {

struct ChunkMeshData {
    std::vector<VoxelVertex> vertices;
    std::vector<uint32_t> indices;
    std::vector<std::array<float, 4>> palette; // RGBA entries indexed by VoxelVertex::paletteIndex()
};

struct ChunkMesh {
    bgfx::VertexBufferHandle vbh = BGFX_INVALID_HANDLE;
    bgfx::IndexBufferHandle ibh = BGFX_INVALID_HANDLE;
    uint32_t indexCount = 0;
    bool valid = false;
    std::vector<std::array<float, 4>> palette;
};

// 10-byte water vertex: 8-byte VoxelVertex base + 2 bytes flow direction
#pragma pack(push, 1)
struct WaterVertex {
    VoxelVertex base;
    int8_t flowDx; // flow direction X, scaled -127..127
    int8_t flowDz; // flow direction Z, scaled -127..127

    uint8_t posX() const { return base.posX(); }
    uint8_t posY() const { return base.posY(); }
    uint8_t posZ() const { return base.posZ(); }
    uint8_t normalIndex() const { return base.normalIndex(); }
};
#pragma pack(pop)

static_assert(sizeof(WaterVertex) == 10, "WaterVertex must be 10 bytes");

struct WaterChunkMeshData {
    std::vector<WaterVertex> vertices;
    std::vector<uint32_t> indices;
    bool hasAlpha = true; // water is always translucent
};

struct WaterChunkMesh {
    bgfx::VertexBufferHandle vbh = BGFX_INVALID_HANDLE;
    bgfx::IndexBufferHandle ibh = BGFX_INVALID_HANDLE;
    uint32_t indexCount = 0;
    bool valid = false;
};

class VoxelMesher {
  public:
    static bgfx::VertexLayout getVertexLayout();
    static bgfx::VertexLayout getWaterVertexLayout();

    // Generate raw mesh data (no bgfx, testable without GPU)
    // lodLevel: 0=1:1, 1=2x stride, 2=4x stride
    static ChunkMeshData meshChunkData(int cx, int cy, int cz, const ChunkedGrid<float>& density,
                                       const ChunkedGrid<Vector4<float, Space::World>>& essence, float threshold = 0.5f,
                                       int lodLevel = 0);

    // Generate bgfx mesh (requires bgfx initialized)
    // lodLevel: 0=1:1, 1=2x stride, 2=4x stride
    static ChunkMesh meshChunk(int cx, int cy, int cz, const ChunkedGrid<float>& density,
                               const ChunkedGrid<Vector4<float, Space::World>>& essence, float threshold = 0.5f,
                               int lodLevel = 0);

    static void destroyMesh(ChunkMesh& mesh);

    // Water mesh generation from FieldLayer water data
    static WaterChunkMeshData meshWaterChunkData(int cx, int cy, int cz, const FieldLayer<float>& waterField,
                                                 const ChunkedGrid<float>& density, float solidThreshold = 0.5f);

    static WaterChunkMesh meshWaterChunk(int cx, int cy, int cz, const FieldLayer<float>& waterField,
                                         const ChunkedGrid<float>& density, float solidThreshold = 0.5f);

    static void destroyWaterMesh(WaterChunkMesh& mesh);
};

} // namespace fabric
