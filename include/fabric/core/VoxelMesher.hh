#pragma once

#include "fabric/core/ChunkedGrid.hh"
#include "fabric/core/Spatial.hh"
#include <bgfx/bgfx.h>
#include <vector>
#include <array>
#include <cstdint>

namespace fabric {

struct VoxelVertex {
    float px, py, pz;    // position
    float nx, ny, nz;    // normal
    float r, g, b, a;    // color (from essence)
};

struct ChunkMeshData {
    std::vector<VoxelVertex> vertices;
    std::vector<uint32_t> indices;
};

struct ChunkMesh {
    bgfx::VertexBufferHandle vbh = BGFX_INVALID_HANDLE;
    bgfx::IndexBufferHandle ibh = BGFX_INVALID_HANDLE;
    uint32_t indexCount = 0;
    bool valid = false;
};

class VoxelMesher {
public:
    static bgfx::VertexLayout getVertexLayout();

    // Generate raw mesh data (no bgfx, testable without GPU)
    static ChunkMeshData meshChunkData(
        int cx, int cy, int cz,
        const ChunkedGrid<float>& density,
        const ChunkedGrid<Vector4<float, Space::World>>& essence,
        float threshold = 0.5f);

    // Generate bgfx mesh (requires bgfx initialized)
    static ChunkMesh meshChunk(
        int cx, int cy, int cz,
        const ChunkedGrid<float>& density,
        const ChunkedGrid<Vector4<float, Space::World>>& essence,
        float threshold = 0.5f);

    static void destroyMesh(ChunkMesh& mesh);
};

} // namespace fabric
