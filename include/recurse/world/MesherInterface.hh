#pragma once

#include "recurse/world/ChunkDensityCache.hh"
#include "recurse/world/SmoothVoxelVertex.hh"

#include <cstdint>
#include <vector>

namespace recurse {

/// CPU-side smooth mesh payload shared by optional smooth meshing paths.
///
/// The current production near-chunk path is Greedy, but comparison and
/// research meshers still exchange geometry through this format so
/// VoxelMeshingSystem can keep them behind one pluggable boundary.
struct SmoothChunkMeshData {
    std::vector<SmoothVoxelVertex> vertices;
    std::vector<uint32_t> indices;

    bool empty() const { return vertices.empty(); }
    size_t vertexBytes() const { return vertices.size() * sizeof(SmoothVoxelVertex); }
    size_t indexBytes() const { return indices.size() * sizeof(uint32_t); }
};

/// Pluggable smooth chunk mesher boundary.
///
/// Short term, this keeps SnapMC and Surface Nets interchangeable while the
/// Greedy-first near-chunk path remains the production default. Long term,
/// smooth implementations should consume mesh-facing semantic or query inputs
/// rather than reaching through storage-specific details.
class MesherInterface {
  public:
    virtual ~MesherInterface() = default;

    /// Build a smooth mesh for one chunk from cached density and material data.
    ///
    /// @param density   Chunk-local scalar field cache including the border ring.
    /// @param material  Chunk-local material cache aligned with density samples.
    /// @param isovalue  Surface threshold applied to the density field.
    /// @param lodLevel  Optional decimation hint; 0 means full resolution.
    /// @return          Smooth mesh payload. Empty means no visible surface.
    virtual SmoothChunkMeshData meshChunk(const ChunkDensityCache& density, const ChunkMaterialCache& material,
                                          float isovalue = 0.5f, int lodLevel = 0) = 0;

    /// Return a stable implementation label for config, logging, and tests.
    virtual const char* name() const = 0;
};

} // namespace recurse
