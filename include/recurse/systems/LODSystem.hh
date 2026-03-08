#pragma once

#include "fabric/core/BgfxHandle.hh"
#include "fabric/core/Camera.hh"
#include "fabric/core/SystemBase.hh"
#include "fabric/simulation/SimulationGrid.hh"
#include "recurse/render/LODGrid.hh"
#include "recurse/render/LODMeshManager.hh"
#include "recurse/render/VoxelRenderer.hh"

#include <bgfx/bgfx.h>
#include <deque>
#include <memory>
#include <unordered_map>
#include <vector>

namespace recurse::systems {

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

    // Configuration
    void setMaxRenderDistance(float blocks) { maxRenderDistance_ = blocks; }
    void setUploadBudget(int sectionsPerFrame) { uploadBudget_ = sectionsPerFrame; }
    void setMaxLODLevel(int level) { maxLODLevel_ = level; }

    // Dependency injection (called by FabricAppDesc or parent system)
    void setSimulationGrid(fabric::simulation::SimulationGrid* grid) { simGrid_ = grid; }
    void setMaterialRegistry(const fabric::simulation::MaterialRegistry* materials);

    // Statistics
    size_t gpuResidentCount() const { return gpuSections_.size(); }
    size_t totalSectionCount() const { return grid_ ? grid_->sectionCount() : 0; }
    size_t visibleSectionCount() const { return visibleSections_.size(); }

  private:
    // Called when a chunk is ready (connected to ChunkPipelineSystem)
    void onChunkReady(int cx, int cy, int cz);

    // Build LOD0 section from SimulationGrid chunk data
    void buildLOD0Section(int cx, int cy, int cz);

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
    fabric::simulation::SimulationGrid* simGrid_ = nullptr;
    const fabric::simulation::MaterialRegistry* materials_ = nullptr;

    // GPU resources
    std::unordered_map<uint64_t, GPUSection> gpuSections_;
    std::vector<VisibleSection> visibleSections_;

    // Pending generation queue
    std::deque<std::tuple<int, int, int>> pendingChunks_;

    // Configuration
    float maxRenderDistance_ = 2048.0f;
    int uploadBudget_ = 50;
    int maxLODLevel_ = 6;
    float baseRadius_ = 96.0f; // 3 chunks * 32 = base LOD0 range
};

} // namespace recurse::systems
