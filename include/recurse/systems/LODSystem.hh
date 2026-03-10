#pragma once

#include "fabric/core/SystemBase.hh"
#include "fabric/render/BgfxHandle.hh"
#include "fabric/render/Camera.hh"
#include "recurse/render/LODGrid.hh"
#include "recurse/render/LODMeshManager.hh"
#include "recurse/render/VoxelRenderer.hh"
#include "recurse/simulation/SimulationGrid.hh"

#include <bgfx/bgfx.h>
#include <deque>
#include <memory>
#include <unordered_map>
#include <vector>

namespace fabric {
class JobScheduler;
}

namespace recurse {
class VoxelRenderer;
class WorldGenerator;
} // namespace recurse

namespace recurse::systems {

struct LODDebugInfo {
    int pendingSections = 0;
    int gpuResidentSections = 0;
    int visibleSections = 0;
    size_t estimatedGpuBytes = 0;
};

class VoxelRenderSystem;

/// ECS system that manages LOD generation, cascade, selection, and rendering.
///
/// LOD Generation: Reads SimulationGrid to build LOD0 sections from chunks.
/// LOD Cascade: When 8 children exist, builds parent via downsampling.
/// LOD Selection: Distance-based level selection: level = floor(log2(distance / baseRadius))
/// LOD Rendering: Submits visible sections to VoxelRenderer.
class LODSystem : public fabric::System<LODSystem> {
  public:
    LODSystem();
    ~LODSystem() override;

    void doInit(fabric::AppContext& ctx) override;
    void doShutdown() override;
    void fixedUpdate(fabric::AppContext& ctx, float fixedDt) override;
    void render(fabric::AppContext& ctx) override;
    void configureDependencies() override;

    // Dependency injection (called by FabricAppDesc or parent system)
    void setSimulationGrid(recurse::simulation::SimulationGrid* grid) { simGrid_ = grid; }
    void setMaterialRegistry(const recurse::simulation::MaterialRegistry* materials);

    // Chunk notification (called by ChunkPipelineSystem after generation/removal)
    void onChunkReady(int cx, int cy, int cz);
    void onChunkRemoved(int cx, int cy, int cz);

    /// Request direct LOD section generation for a chunk outside the full-res
    /// ring. Uses WorldGenerator::sampleMaterial() instead of SimulationGrid.
    void requestDirectLOD(int cx, int cy, int cz);

    // Statistics
    size_t gpuResidentCount() const { return gpuSections_.size(); }
    size_t totalSectionCount() const { return grid_ ? grid_->sectionCount() : 0; }
    size_t visibleSectionCount() const { return visibleSections_.size(); }
    LODDebugInfo debugInfo() const;

  private:
    // Distance-based LOD selection + frustum culling
    struct VisibleSection {
        const LODSection* section;
        float distance;
        LODSectionKey key;
    };
    void selectVisibleSections(const fabric::Camera& camera, float baseRadius);

    // GPU mesh management
    struct GPUSection {
        fabric::BgfxHandle<bgfx::VertexBufferHandle> vbh;
        fabric::BgfxHandle<bgfx::IndexBufferHandle> ibh;
        uint32_t vertexCount = 0;
        uint32_t indexCount = 0;
        std::vector<std::array<float, 4>> palette;
        bool resident = false;
    };
    void uploadSection(LODSectionKey key, const recurse::LODMeshManager::MeshResult& mesh);
    void releaseGPUSection(LODSectionKey key);

    // Core components
    std::unique_ptr<LODGrid> grid_;
    std::unique_ptr<LODMeshManager> meshManager_;

    // References to other systems
    recurse::simulation::SimulationGrid* simGrid_ = nullptr;
    const recurse::simulation::MaterialRegistry* materials_ = nullptr;
    fabric::JobScheduler* scheduler_ = nullptr;
    recurse::VoxelRenderer* voxelRenderer_ = nullptr;
    recurse::WorldGenerator* worldGen_ = nullptr;

    // GPU resources
    std::unordered_map<uint64_t, GPUSection> gpuSections_;
    std::vector<VisibleSection> visibleSections_;

    // Pending generation queues
    std::deque<std::tuple<int, int, int>> pendingChunks_;
    std::deque<std::tuple<int, int, int>> pendingDirectChunks_;

    // Configuration (read from TOML in doInit; defaults match pre-config values)
    int uploadBudget_ = 50;
    int maxLODLevel_ = 6;
    float baseRadius_ = 10.0f;
};

} // namespace recurse::systems
