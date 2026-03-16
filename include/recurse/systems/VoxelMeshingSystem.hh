#pragma once

#include "fabric/core/CompilerHints.hh"
#include "fabric/core/SystemBase.hh"
#include "fabric/world/ChunkCoord.hh"
#include "fabric/world/ChunkedGrid.hh"
#include "recurse/render/VoxelRenderer.hh"
#include "recurse/simulation/VoxelConstants.hh"
#include "recurse/simulation/VoxelMaterial.hh"

#include <array>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <unordered_set>

namespace fabric {
class JobScheduler;
} // namespace fabric

namespace recurse {
class EssencePalette;
} // namespace recurse

namespace recurse::simulation {
class ChunkActivityTracker;
class MaterialRegistry;
class SimulationGrid;
} // namespace recurse::simulation

namespace recurse {
class SnapMCMesher;
} // namespace recurse

namespace recurse::systems {

/// Pre-resolved buffer pointers for a chunk and its 6 face-adjacent neighbors.
/// Eliminates per-cell hash lookups during meshing. Built once per chunk before
/// the meshing loop; all pointers are valid for the current epoch.
struct MeshingChunkContext {
    using Buffer = std::array<recurse::simulation::VoxelCell, recurse::simulation::K_CHUNK_VOLUME>;

    const Buffer* self = nullptr;
    const Buffer* neighbors[6] = {}; ///< +X, -X, +Y, -Y, +Z, -Z (nullptr if absent)
    const recurse::EssencePalette* palette = nullptr;
    recurse::simulation::VoxelCell selfFill{};        ///< Fill value for non-materialized center chunk
    recurse::simulation::VoxelCell neighborFill[6]{}; ///< Fill values for non-materialized neighbors
    int cx, cy, cz;

    /// Read a cell at local coordinates relative to the center chunk.
    /// Coordinates outside [0, K_CHUNK_SIZE) are resolved via neighbor buffers
    /// when possible, falling back to Fallback::readCell for edge/corner cells.
    /// Fallback is templated to avoid requiring SimulationGrid.hh in the header.
    template <typename Fallback>
    FABRIC_ALWAYS_INLINE recurse::simulation::VoxelCell readLocal(int lx, int ly, int lz,
                                                                  const Fallback* fallback) const {
        if (lx >= 0 && lx < recurse::simulation::K_CHUNK_SIZE && ly >= 0 && ly < recurse::simulation::K_CHUNK_SIZE &&
            lz >= 0 && lz < recurse::simulation::K_CHUNK_SIZE) {
            if (self)
                return (*self)[lx + ly * recurse::simulation::K_CHUNK_SIZE +
                               lz * recurse::simulation::K_CHUNK_SIZE * recurse::simulation::K_CHUNK_SIZE];
            return selfFill;
        }

        int outCount = (lx < 0 || lx >= recurse::simulation::K_CHUNK_SIZE) +
                       (ly < 0 || ly >= recurse::simulation::K_CHUNK_SIZE) +
                       (lz < 0 || lz >= recurse::simulation::K_CHUNK_SIZE);
        if (outCount == 1) {
            int face = -1;
            int nlx = lx, nly = ly, nlz = lz;
            if (lx >= recurse::simulation::K_CHUNK_SIZE) {
                face = 0;
                nlx = lx - recurse::simulation::K_CHUNK_SIZE;
            } else if (lx < 0) {
                face = 1;
                nlx = lx + recurse::simulation::K_CHUNK_SIZE;
            } else if (ly >= recurse::simulation::K_CHUNK_SIZE) {
                face = 2;
                nly = ly - recurse::simulation::K_CHUNK_SIZE;
            } else if (ly < 0) {
                face = 3;
                nly = ly + recurse::simulation::K_CHUNK_SIZE;
            } else if (lz >= recurse::simulation::K_CHUNK_SIZE) {
                face = 4;
                nlz = lz - recurse::simulation::K_CHUNK_SIZE;
            } else {
                face = 5;
                nlz = lz + recurse::simulation::K_CHUNK_SIZE;
            }
            if (neighbors[face])
                return (*neighbors[face])[nlx + nly * recurse::simulation::K_CHUNK_SIZE +
                                          nlz * recurse::simulation::K_CHUNK_SIZE * recurse::simulation::K_CHUNK_SIZE];
            return neighborFill[face];
        }

        if (fallback) {
            int wx = cx * recurse::simulation::K_CHUNK_SIZE + lx;
            int wy = cy * recurse::simulation::K_CHUNK_SIZE + ly;
            int wz = cz * recurse::simulation::K_CHUNK_SIZE + lz;
            return fallback->readCell(wx, wy, wz);
        }
        return {};
    }
};

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
    MeshingChunkContext buildMeshingContext(const fabric::ChunkCoord& coord) const;
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
