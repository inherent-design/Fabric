#include "recurse/systems/VoxelMeshingSystem.hh"
#include "fabric/world/ChunkCoordUtils.hh"

#include "fabric/core/AppContext.hh"
#include "fabric/core/Log.hh"
#include "fabric/core/SystemRegistry.hh"
#include "fabric/simulation/ChunkActivityTracker.hh"
#include "fabric/simulation/SimulationGrid.hh"
#include "fabric/simulation/VoxelMaterial.hh"
#include "fabric/utils/Profiler.hh"
#include "fabric/world/ChunkedGrid.hh"
#include "recurse/render/VertexPool.hh"
#include "recurse/systems/ShadowRenderSystem.hh"
#include "recurse/systems/VoxelSimulationSystem.hh"
#include "recurse/world/ChunkDensityCache.hh"
#include "recurse/world/SmoothVoxelVertex.hh"
#include "recurse/world/SnapMCMesher.hh"

#include <bgfx/bgfx.h>
#include <set>
#include <unordered_map>
#include <vector>

namespace recurse::systems {

using fabric::ChunkedGrid;
using fabric::K_HORIZONTAL_NEIGHBORS;
using fabric::kChunkSize;

namespace {

fabric::ChunkCoord toChunkCoord(const fabric::simulation::ChunkPos& pos) {
    return fabric::ChunkCoord{pos.x, pos.y, pos.z};
}

uint16_t unpackMaterialId(uint32_t packedMaterial) {
    return static_cast<uint16_t>(packedMaterial & 0xFFFFu);
}

uint32_t packSmoothMaterialForShader(uint16_t paletteIndex, uint8_t aoLevel = 3) {
    return recurse::SmoothVoxelVertex::packMaterial(paletteIndex, aoLevel, 0);
}

} // namespace

VoxelMeshingSystem::VoxelMeshingSystem() : mesher_(std::make_unique<SnapMCMesher>()) {}

VoxelMeshingSystem::~VoxelMeshingSystem() = default;

void VoxelMeshingSystem::doInit(fabric::AppContext& ctx) {
    simSystem_ = ctx.systemRegistry.get<VoxelSimulationSystem>();
    if (simSystem_) {
        simGrid_ = &simSystem_->simulationGrid();
        activityTracker_ = &simSystem_->activityTracker();
    }

    if (!mesher_)
        mesher_ = std::make_unique<SnapMCMesher>();

    SmoothVertexPool::Config poolConfig;
    gpuUploadEnabled_ = (ctx.renderCaps != nullptr);
    poolConfig.cpuOnly = !gpuUploadEnabled_;

    vertexPool_ = std::make_unique<SmoothVertexPool>();
    vertexPool_->init(poolConfig);

    FABRIC_LOG_INFO("VoxelMeshingSystem initialized (budget={}, simBound={}, trackerBound={}, gpuUpload={})",
                    meshBudget_, simGrid_ != nullptr, activityTracker_ != nullptr, gpuUploadEnabled_);
}

void VoxelMeshingSystem::doShutdown() {
    for (auto& [_, gpuMesh] : gpuMeshes_)
        destroyChunkMesh(gpuMesh);
    gpuMeshes_.clear();

    if (vertexPool_) {
        vertexPool_->shutdown();
        vertexPool_.reset();
    }

    mesher_.reset();
    simSystem_ = nullptr;
    simGrid_ = nullptr;
    activityTracker_ = nullptr;
    gpuUploadEnabled_ = false;

    FABRIC_LOG_INFO("VoxelMeshingSystem shutdown");
}

void VoxelMeshingSystem::clearAllMeshes() {
    const auto count = gpuMeshes_.size();
    for (auto& [_, gpuMesh] : gpuMeshes_)
        destroyChunkMesh(gpuMesh);
    gpuMeshes_.clear();
    FABRIC_LOG_INFO("VoxelMeshingSystem: cleared {} GPU meshes", count);
}

void VoxelMeshingSystem::render(fabric::AppContext& /*ctx*/) {
    FABRIC_ZONE_SCOPED_N("voxel_meshing");
    processFrame();
}

void VoxelMeshingSystem::configureDependencies() {
    after<VoxelSimulationSystem>();
    before<ShadowRenderSystem>();
}

void VoxelMeshingSystem::setSimulationGrid(fabric::simulation::SimulationGrid* grid) {
    simGrid_ = grid;
}

void VoxelMeshingSystem::setActivityTracker(fabric::simulation::ChunkActivityTracker* tracker) {
    activityTracker_ = tracker;
}

void VoxelMeshingSystem::processFrame() {
    if ((!simGrid_ || !activityTracker_) && simSystem_) {
        simGrid_ = &simSystem_->simulationGrid();
        activityTracker_ = &simSystem_->activityTracker();
    }

    if (!activityTracker_ || !simGrid_ || !mesher_)
        return;

    std::vector<fabric::simulation::ActiveChunkEntry> activeChunks;
    {
        FABRIC_ZONE_SCOPED_N("mesh_collect_active");
        activeChunks = activityTracker_->collectActiveChunks(meshBudget_);
    }

    // Log chunk activity for this frame
    FABRIC_LOG_DEBUG("VoxelMeshingSystem: {} active chunks to mesh (budget={})", activeChunks.size(), meshBudget_);

    for (const auto& entry : activeChunks) {
        const auto coord = toChunkCoord(entry.pos);
        bool wasAlreadyMeshed = gpuMeshes_.find(coord) != gpuMeshes_.end();
        {
            FABRIC_ZONE_SCOPED_N("chunk_remesh");
            meshChunk(coord);
        }
        // Log if this was a remesh (neighbor notification triggered)
        if (wasAlreadyMeshed) {
            FABRIC_LOG_DEBUG("Remeshed chunk ({},{},{}) - likely neighbor notification", coord.x, coord.y, coord.z);
        }
        activityTracker_->putToSleep(entry.pos);
    }

    // Log GPU mesh statistics
    FABRIC_LOG_DEBUG("VoxelMeshingSystem: {} GPU meshes active, {} vertices, {} indices", gpuMeshes_.size(),
                     vertexBufferSize(), indexBufferSize());
}

void VoxelMeshingSystem::meshChunk(const fabric::ChunkCoord& coord) {
    // Verify the chunk exists in the simulation grid before meshing.
    if (!simGrid_->hasChunk(coord.x, coord.y, coord.z)) {
        FABRIC_LOG_DEBUG("Meshing skipped ({},{},{}): chunk not in grid", coord.x, coord.y, coord.z);
        return;
    }

    // Check horizontal face-adjacent neighbors (±X, ±Z) before meshing.
    // Missing neighbors return air from readCell, creating boundary artifacts.
    // Y neighbors (±Y) are checked but don't block meshing - flat terrain at Y=0
    // legitimately has no Y neighbors, and sampling air above/below is correct.
    // When horizontal neighbors load later, notifyBoundaryChange() re-triggers meshing.
    if (requireNeighborsForMeshing_) {
        // Horizontal neighbors (+/-X, +/-Z) - MUST exist to avoid X/Z boundary gaps
        for (int d = 0; d < 4; ++d) {
            if (!simGrid_->hasChunk(coord.x + K_HORIZONTAL_NEIGHBORS[d][0], coord.y,
                                    coord.z + K_HORIZONTAL_NEIGHBORS[d][2])) {
                FABRIC_LOG_DEBUG("Meshing deferred ({},{},{}): horizontal neighbor ({},{},{}) missing", coord.x,
                                 coord.y, coord.z, coord.x + K_HORIZONTAL_NEIGHBORS[d][0], coord.y,
                                 coord.z + K_HORIZONTAL_NEIGHBORS[d][2]);
                return; // Horizontal neighbor missing - defer meshing
            }
        }
        // Y neighbors (±Y) - log if missing but don't block (air above/below is valid for flat terrain)
        if (!simGrid_->hasChunk(coord.x, coord.y - 1, coord.z) || !simGrid_->hasChunk(coord.x, coord.y + 1, coord.z)) {
            FABRIC_LOG_DEBUG("Meshing ({},{},{}): Y neighbor missing, sampling air at Y boundary", coord.x, coord.y,
                             coord.z);
        }
    }

    // Build density + material grids from SimulationGrid.
    // Sample ±1 voxel around chunk for ChunkDensityCache boundary.
    // Total: 32 + 2 = 34^3 region
    ChunkedGrid<float> densityGrid;
    ChunkedGrid<uint16_t> materialGrid;

    const int baseX = coord.x * kChunkSize;
    const int baseY = coord.y * kChunkSize;
    const int baseZ = coord.z * kChunkSize;

    constexpr int K_SAMPLE_MARGIN = 1; // 1 for cache boundary
    for (int lz = -K_SAMPLE_MARGIN; lz <= kChunkSize + K_SAMPLE_MARGIN; ++lz) {
        for (int ly = -K_SAMPLE_MARGIN; ly <= kChunkSize + K_SAMPLE_MARGIN; ++ly) {
            for (int lx = -K_SAMPLE_MARGIN; lx <= kChunkSize + K_SAMPLE_MARGIN; ++lx) {
                const int wx = baseX + lx;
                const int wy = baseY + ly;
                const int wz = baseZ + lz;

                const auto cell = simGrid_->readCell(wx, wy, wz);
                const float density = (cell.materialId == fabric::simulation::material_ids::AIR) ? 0.0f : 1.0f;
                densityGrid.set(wx, wy, wz, density);
                materialGrid.set(wx, wy, wz, cell.materialId);
            }
        }
    }

    // No blur - use binary density directly for sharp corners.
    // The blur was causing chamfered edges by creating intermediate values (0.33, 0.66)
    // at solid-air boundaries.
    ChunkDensityCache densityCache;
    ChunkMaterialCache materialCache;
    densityCache.build(coord.x, coord.y, coord.z, densityGrid);
    materialCache.build(coord.x, coord.y, coord.z, materialGrid);

    auto meshData = mesher_->meshChunk(densityCache, materialCache, 0.5f, 0);

    auto existing = gpuMeshes_.find(coord);
    if (meshData.empty() || meshData.indices.empty()) {
        if (existing != gpuMeshes_.end()) {
            destroyChunkMesh(existing->second);
            gpuMeshes_.erase(existing);
        }
        return;
    }

    std::vector<recurse::SmoothVoxelVertex> gpuVertices;
    gpuVertices.reserve(meshData.vertices.size());

    // Collect unique materials first for deterministic palette ordering
    std::set<uint16_t> uniqueMaterials;
    for (const auto& smoothVertex : meshData.vertices) {
        uniqueMaterials.insert(unpackMaterialId(smoothVertex.material));
    }

    // Build deterministic palette (sorted by material ID ensures consistent colors across chunks)
    std::vector<std::array<float, 4>> palette;
    palette.reserve(uniqueMaterials.size());
    std::unordered_map<uint16_t, uint16_t> paletteLookup;

    for (uint16_t materialId : uniqueMaterials) {
        paletteLookup[materialId] = static_cast<uint16_t>(palette.size());
        palette.push_back(materialColor(materialId));
    }

    // Transform vertices with deterministic palette indices
    for (const auto& smoothVertex : meshData.vertices) {
        const uint16_t materialId = unpackMaterialId(smoothVertex.material);
        recurse::SmoothVoxelVertex vertex = smoothVertex;
        vertex.material = packSmoothMaterialForShader(paletteLookup[materialId], 3);
        gpuVertices.push_back(vertex);
    }

    ChunkGPUMesh gpuMesh;
    gpuMesh.vertexCount = static_cast<uint32_t>(gpuVertices.size());
    gpuMesh.indexCount = static_cast<uint32_t>(meshData.indices.size());
    gpuMesh.mesh.indexCount = gpuMesh.indexCount;
    gpuMesh.mesh.palette = std::move(palette);
    gpuMesh.mesh.valid = (gpuMesh.vertexCount > 0 && gpuMesh.mesh.indexCount > 0);
    gpuMesh.valid = gpuMesh.mesh.valid;

    if (gpuUploadEnabled_ && gpuMesh.valid) {
        gpuMesh.mesh.vbh = bgfx::createVertexBuffer(
            bgfx::copy(gpuVertices.data(),
                       static_cast<uint32_t>(gpuVertices.size() * sizeof(recurse::SmoothVoxelVertex))),
            recurse::SmoothVoxelVertex::getVertexLayout());
        if (!bgfx::isValid(gpuMesh.mesh.vbh)) {
            FABRIC_LOG_ERROR("Failed to create vertex buffer for chunk ({},{},{})", coord.x, coord.y, coord.z);
            gpuMesh.valid = false;
            gpuMesh.mesh.valid = false;
            return;
        }

        gpuMesh.mesh.ibh = bgfx::createIndexBuffer(
            bgfx::copy(meshData.indices.data(), static_cast<uint32_t>(meshData.indices.size() * sizeof(uint32_t))),
            BGFX_BUFFER_INDEX32);
        if (!bgfx::isValid(gpuMesh.mesh.ibh)) {
            FABRIC_LOG_ERROR("Failed to create index buffer for chunk ({},{},{})", coord.x, coord.y, coord.z);
            bgfx::destroy(gpuMesh.mesh.vbh);
            gpuMesh.mesh.vbh = BGFX_INVALID_HANDLE;
            gpuMesh.valid = false;
            gpuMesh.mesh.valid = false;
            return;
        }
    }

    if (!gpuMesh.valid) {
        if (existing != gpuMeshes_.end()) {
            destroyChunkMesh(existing->second);
            gpuMeshes_.erase(existing);
        }
        FABRIC_LOG_DEBUG("Meshing failed ({},{},{}): invalid GPU mesh", coord.x, coord.y, coord.z);
        return;
    }

    if (existing != gpuMeshes_.end()) {
        destroyChunkMesh(existing->second);
        existing->second = std::move(gpuMesh);
    } else {
        gpuMeshes_.emplace(coord, std::move(gpuMesh));
    }

    // Log successful meshing with statistics
    const auto& mesh = gpuMeshes_.at(coord);
    FABRIC_LOG_DEBUG("Meshing complete ({},{},{}): vertices={} indices={} materials={}", coord.x, coord.y, coord.z,
                     mesh.vertexCount, mesh.indexCount, mesh.mesh.palette.size());

    // Log palette entries at TRACE level for detailed debugging
    for (size_t i = 0; i < mesh.mesh.palette.size(); ++i) {
        const auto& c = mesh.mesh.palette[i];
        FABRIC_LOG_TRACE("  palette[{}]: rgba=({:.2f},{:.2f},{:.2f},{:.2f})", i, c[0], c[1], c[2], c[3]);
    }
}

void VoxelMeshingSystem::destroyChunkMesh(ChunkGPUMesh& gpuMesh) {
    if (bgfx::isValid(gpuMesh.mesh.vbh))
        bgfx::destroy(gpuMesh.mesh.vbh);
    if (bgfx::isValid(gpuMesh.mesh.ibh))
        bgfx::destroy(gpuMesh.mesh.ibh);

    gpuMesh.mesh.vbh = BGFX_INVALID_HANDLE;
    gpuMesh.mesh.ibh = BGFX_INVALID_HANDLE;
    gpuMesh.mesh.indexCount = 0;
    gpuMesh.mesh.palette.clear();
    gpuMesh.mesh.valid = false;

    gpuMesh.vertexCount = 0;
    gpuMesh.indexCount = 0;
    gpuMesh.valid = false;
}

std::array<float, 4> VoxelMeshingSystem::materialColor(uint16_t materialId) const {
    using namespace fabric::simulation;
    switch (materialId) {
        case material_ids::STONE:
            return {0.56f, 0.56f, 0.60f, 1.0f};
        case material_ids::DIRT:
            return {0.43f, 0.30f, 0.20f, 1.0f};
        case material_ids::SAND:
            return {0.84f, 0.77f, 0.52f, 1.0f};
        case material_ids::WATER:
            return {0.24f, 0.45f, 0.82f, 1.0f};
        case material_ids::GRAVEL:
            return {0.47f, 0.45f, 0.43f, 1.0f};
        case material_ids::AIR:
        default:
            return {0.0f, 0.0f, 0.0f, 0.0f};
    }
}

size_t VoxelMeshingSystem::pendingMeshCount() const {
    if (!activityTracker_)
        return 0;
    // Get count of non-sleeping chunks that need meshing
    return activityTracker_->collectActiveChunks().size();
}

size_t VoxelMeshingSystem::vertexBufferSize() const {
    size_t total = 0;
    for (const auto& [coord, gpuMesh] : gpuMeshes_) {
        total += gpuMesh.vertexCount;
    }
    return total;
}

size_t VoxelMeshingSystem::indexBufferSize() const {
    size_t total = 0;
    for (const auto& [coord, gpuMesh] : gpuMeshes_) {
        total += gpuMesh.indexCount;
    }
    return total;
}

} // namespace recurse::systems
