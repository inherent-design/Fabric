#pragma once

#include "fabric/core/ChunkCoord.hh"
#include "fabric/core/SystemBase.hh"
#include "recurse/render/VoxelRenderer.hh"

#include <array>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <unordered_set>

namespace recurse::simulation {
class ChunkActivityTracker;
class MaterialRegistry;
class SimulationGrid;
} // namespace recurse::simulation

namespace recurse {
class SnapMCMesher;
} // namespace recurse

// SmoothVertexPool is a type alias, cannot forward-declare
#include "recurse/render/VertexPool.hh"

namespace recurse::systems {

class ShadowRenderSystem;
class VoxelSimulationSystem;

struct ChunkGPUMesh {
    recurse::ChunkMesh mesh;
    uint32_t vertexCount = 0;
    uint32_t indexCount = 0;
    bool valid = false;
};

/// Bridges simulation activity flags to mesh updates via SnapMC meshing.
/// Runs in PreRender phase. Collects active chunks from ChunkActivityTracker,
/// sorts by priority, and meshes up to a per-frame chunk budget.
class VoxelMeshingSystem : public fabric::System<VoxelMeshingSystem> {
  public:
    VoxelMeshingSystem();
    ~VoxelMeshingSystem() override;

    VoxelMeshingSystem(const VoxelMeshingSystem&) = delete;
    VoxelMeshingSystem& operator=(const VoxelMeshingSystem&) = delete;

    void doInit(fabric::AppContext& ctx) override;
    void doShutdown() override;
    void render(fabric::AppContext& ctx) override;
    void configureDependencies() override;

    void setSimulationGrid(recurse::simulation::SimulationGrid* grid);
    void setActivityTracker(recurse::simulation::ChunkActivityTracker* tracker);
    void setMeshBudget(int budget) { meshBudget_ = budget; }
    int meshBudget() const { return meshBudget_; }

    /// When true, requires all 6 face-adjacent neighbors to exist before meshing.
    /// This prevents geometry gaps at chunk boundaries but requires notification
    /// system to re-mesh when neighbors load. Default: false (mesh immediately).
    void setRequireNeighborsForMeshing(bool require) { requireNeighborsForMeshing_ = require; }

    /// Process one frame of meshing. Called by render(); exposed for testing.
    void processFrame();

    /// Clear all GPU meshes (for world reset)
    void clearAllMeshes();

    const auto& gpuMeshes() const { return gpuMeshes_; }

    /// Statistics accessors for debug panel.
    size_t gpuMeshCount() const { return gpuMeshes_.size(); }
    size_t pendingMeshCount() const;
    size_t vertexBufferSize() const;
    size_t indexBufferSize() const;

  private:
    void meshChunk(const fabric::ChunkCoord& coord);
    void destroyChunkMesh(ChunkGPUMesh& gpuMesh);
    std::array<float, 4> materialColor(uint16_t materialId) const;

    VoxelSimulationSystem* simSystem_ = nullptr;
    recurse::simulation::SimulationGrid* simGrid_ = nullptr;
    recurse::simulation::ChunkActivityTracker* activityTracker_ = nullptr;
    const recurse::simulation::MaterialRegistry* materials_ = nullptr;
    std::unique_ptr<recurse::SnapMCMesher> mesher_;
    std::unique_ptr<recurse::SmoothVertexPool> vertexPool_;

    std::unordered_map<fabric::ChunkCoord, ChunkGPUMesh, fabric::ChunkCoordHash> gpuMeshes_;
    std::unordered_set<fabric::ChunkCoord, fabric::ChunkCoordHash> emptyChunks_;
    int meshBudget_ = 50; // Increased from 3 to handle initial load in one frame
    bool gpuUploadEnabled_ = false;
    bool requireNeighborsForMeshing_ = true;
};

} // namespace recurse::systems
