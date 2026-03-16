#pragma once

#include "fabric/core/SystemBase.hh"
#include "fabric/world/ChunkCoord.hh"
#include "recurse/render/VoxelRenderer.hh"

#include <array>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <unordered_set>

namespace fabric {
class JobScheduler;
} // namespace fabric

namespace recurse::simulation {
class ChunkActivityTracker;
class MaterialRegistry;
class SimulationGrid;
} // namespace recurse::simulation

namespace recurse {
class SnapMCMesher;
} // namespace recurse

namespace recurse::systems {

struct MeshingDebugInfo {
    int chunksMeshedThisFrame = 0;
    int emptyChunksSkipped = 0;
    int budgetRemaining = 0;
};

class ShadowRenderSystem;
class VoxelSimulationSystem;

struct ChunkGPUMesh {
    recurse::ChunkMesh mesh;
    uint32_t vertexCount = 0;
    uint32_t indexCount = 0;
    bool valid = false;
};

/// CPU-side mesh output from parallel generation. Consumed by sequential GPU upload.
struct CPUMeshResult {
    std::vector<recurse::SmoothVoxelVertex> vertices;
    std::vector<uint32_t> indices;
    std::vector<std::array<float, 4>> palette;
    bool valid = false;
    bool deferred = false; ///< True if neighbor check deferred meshing
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

    void onWorldBegin();
    void onWorldEnd();

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

    /// Remove GPU mesh for a single chunk (for streaming unload)
    void removeChunkMesh(const fabric::ChunkCoord& coord);

    const auto& gpuMeshes() const { return gpuMeshes_; }

    /// Statistics accessors for debug panel.
    size_t gpuMeshCount() const { return gpuMeshes_.size(); }
    size_t pendingMeshCount() const;
    size_t vertexBufferSize() const;
    size_t indexBufferSize() const;
    MeshingDebugInfo debugInfo() const;

  private:
    CPUMeshResult generateMeshCPU(const fabric::ChunkCoord& coord) const;
    void uploadMeshResult(const fabric::ChunkCoord& coord, CPUMeshResult&& result);
    void destroyChunkMesh(ChunkGPUMesh& gpuMesh);
    std::array<float, 4> materialColor(uint16_t materialId) const;

    VoxelSimulationSystem* simSystem_ = nullptr;
    recurse::simulation::SimulationGrid* simGrid_ = nullptr;
    recurse::simulation::ChunkActivityTracker* activityTracker_ = nullptr;
    const recurse::simulation::MaterialRegistry* materials_ = nullptr;
    fabric::JobScheduler* scheduler_ = nullptr;
    std::unique_ptr<recurse::SnapMCMesher> mesher_;

    std::unordered_map<fabric::ChunkCoord, ChunkGPUMesh, fabric::ChunkCoordHash> gpuMeshes_;
    std::unordered_set<fabric::ChunkCoord, fabric::ChunkCoordHash> emptyChunks_;
    int meshBudget_ = 50; // Increased from 3 to handle initial load in one frame
    bool gpuUploadEnabled_ = false;
    bool requireNeighborsForMeshing_ = true;

    int meshedThisFrame_ = 0;
    int emptySkippedThisFrame_ = 0;
};

} // namespace recurse::systems
