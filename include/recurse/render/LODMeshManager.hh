#pragma once

#include "recurse/render/LODGrid.hh"
#include "recurse/world/VoxelVertex.hh"

#include <array>
#include <vector>

namespace recurse::simulation {
class MaterialRegistry;
}

namespace recurse {

/// Manages mesh generation for LOD sections using the voxel-first packed mesh
/// contract shared with the default near path.
class LODMeshManager {
  public:
    struct MeshResult {
        std::vector<VoxelVertex> vertices;
        std::vector<uint32_t> indices;
        std::vector<std::array<float, 4>> palette;
        bool empty() const { return vertices.empty() || indices.empty(); }
    };

    explicit LODMeshManager(LODGrid& grid, const recurse::simulation::MaterialRegistry& materials);
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
    LODGrid& grid_;
    const recurse::simulation::MaterialRegistry& materials_;
};

} // namespace recurse
