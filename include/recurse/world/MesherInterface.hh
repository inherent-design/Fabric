#pragma once

#include "recurse/world/ChunkDensityCache.hh"
#include "recurse/world/SmoothVoxelVertex.hh"

#include <cstdint>
#include <vector>

namespace recurse {

struct SmoothChunkMeshData {
    std::vector<SmoothVoxelVertex> vertices;
    std::vector<uint32_t> indices;

    bool empty() const { return vertices.empty(); }
    size_t vertexBytes() const { return vertices.size() * sizeof(SmoothVoxelVertex); }
    size_t indexBytes() const { return indices.size() * sizeof(uint32_t); }
};

class MesherInterface {
  public:
    virtual ~MesherInterface() = default;

    virtual SmoothChunkMeshData meshChunk(const ChunkDensityCache& density, const ChunkMaterialCache& material,
                                          float isovalue = 0.5f, int lodLevel = 0) = 0;

    virtual const char* name() const = 0;
};

} // namespace recurse
