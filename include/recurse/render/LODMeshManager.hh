#pragma once

#include "recurse/render/LODGrid.hh"
#include "recurse/world/SmoothVoxelVertex.hh"

#include <array>
#include <memory>
#include <vector>

namespace fabric::simulation {
class MaterialRegistry;
}

namespace recurse {

class SnapMCMesher;
class ChunkDensityCache;
class ChunkMaterialCache;

/// Manages mesh generation for LOD sections using the SnapMC mesher.
/// Each LODSection produces a MeshResult (vertices + indices + palette)
/// which can be uploaded to GPU by LODSystem.
class LODMeshManager {
  public:
    struct MeshResult {
        std::vector<SmoothVoxelVertex> vertices;
        std::vector<uint32_t> indices;
        std::vector<std::array<float, 4>> palette;
        bool empty() const { return vertices.empty() || indices.empty(); }
    };

    explicit LODMeshManager(LODGrid& grid, const fabric::simulation::MaterialRegistry& materials);
    ~LODMeshManager();

    /// Generate mesh for a single LOD section.
    /// Returns empty MeshResult if section has no solid voxels.
    MeshResult meshSection(const LODSection& section);

    /// Rebuild meshes for dirty sections, up to the per-frame budget.
    /// Returns number of sections processed.
    int rebuildDirty(int budget);

    /// Total count of pending (dirty) sections.
    size_t pendingCount() const;

  private:
    /// Build density and material caches from a LODSection for meshing.
    void buildCaches(const LODSection& section, ChunkDensityCache& density, ChunkMaterialCache& material);

    /// Convert materialId to RGBA color from the MaterialRegistry.
    std::array<float, 4> materialColor(uint16_t materialId) const;

    LODGrid& grid_;
    const fabric::simulation::MaterialRegistry& materials_;
    std::unique_ptr<SnapMCMesher> mesher_;
};

} // namespace recurse
