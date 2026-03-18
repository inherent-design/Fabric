#include "recurse/systems/VoxelMeshingSystem.hh"
#include "fabric/world/ChunkCoordUtils.hh"

#include "fabric/core/AppContext.hh"
#include "fabric/core/SystemRegistry.hh"
#include "fabric/core/WorldLifecycle.hh"
#include "fabric/log/Log.hh"
#include "fabric/platform/JobScheduler.hh"
#include "fabric/utils/Profiler.hh"
#include "fabric/world/ChunkedGrid.hh"
#include "recurse/simulation/ChunkActivityTracker.hh"
#include "recurse/simulation/MaterialRegistry.hh"
#include "recurse/simulation/SimulationGrid.hh"
#include "recurse/simulation/VoxelMaterial.hh"
#include "recurse/systems/ShadowRenderSystem.hh"
#include "recurse/systems/VoxelSimulationSystem.hh"
#include "recurse/world/ChunkDensityCache.hh"
#include "recurse/world/EssencePalette.hh"
#include "recurse/world/SmoothVoxelVertex.hh"
#include "recurse/world/SnapMCMesher.hh"

#include <bgfx/bgfx.h>
#include <set>
#include <unordered_map>
#include <vector>

namespace recurse::systems {

using fabric::ChunkedGrid;
using fabric::K_HORIZONTAL_NEIGHBORS;
using recurse::simulation::K_CHUNK_SIZE;

namespace {

uint16_t unpackMaterialId(uint32_t packedMaterial) {
    return static_cast<uint16_t>(packedMaterial & 0xFFFFu);
}

} // namespace

VoxelMeshingSystem::VoxelMeshingSystem() : mesher_(std::make_unique<SnapMCMesher>()) {}

VoxelMeshingSystem::~VoxelMeshingSystem() {
    for (auto& [_, gpuMesh] : gpuMeshes_)
        destroyChunkMesh(gpuMesh);
}

void VoxelMeshingSystem::doInit(fabric::AppContext& ctx) {
    if (auto* wl = ctx.worldLifecycle) {
        wl->registerParticipant([this]() { onWorldBegin(); }, [this]() { onWorldEnd(); });
    }
    simSystem_ = ctx.systemRegistry.get<VoxelSimulationSystem>();
    if (simSystem_) {
        simGrid_ = &simSystem_->simulationGrid();
        activityTracker_ = &simSystem_->activityTracker();
        materials_ = &simSystem_->materials();
        scheduler_ = &simSystem_->scheduler();
    }

    if (!mesher_)
        mesher_ = std::make_unique<SnapMCMesher>();

    gpuUploadEnabled_ = (ctx.renderCaps != nullptr);

    FABRIC_LOG_INFO("VoxelMeshingSystem initialized (budget={}, simBound={}, trackerBound={}, gpuUpload={})",
                    meshBudget_, simGrid_ != nullptr, activityTracker_ != nullptr, gpuUploadEnabled_);
}

void VoxelMeshingSystem::doShutdown() {
    clearAllMeshes();
    mesher_.reset();
    simSystem_ = nullptr;
    simGrid_ = nullptr;
    activityTracker_ = nullptr;
    materials_ = nullptr;
    scheduler_ = nullptr;
    gpuUploadEnabled_ = false;

    FABRIC_LOG_INFO("VoxelMeshingSystem shutdown");
}

void VoxelMeshingSystem::clearAllMeshes() {
    const auto count = gpuMeshes_.size();
    for (auto& [_, gpuMesh] : gpuMeshes_)
        destroyChunkMesh(gpuMesh);
    gpuMeshes_.clear();
    emptyChunks_.clear();
    pendingMeshCount_ = 0;
    vertexBufferSize_ = 0;
    indexBufferSize_ = 0;
    FABRIC_LOG_INFO("VoxelMeshingSystem: cleared {} GPU meshes", count);
}

void VoxelMeshingSystem::removeChunkMesh(const fabric::ChunkCoord& coord) {
    auto it = gpuMeshes_.find(coord);
    if (it != gpuMeshes_.end()) {
        destroyChunkMesh(it->second);
        gpuMeshes_.erase(it);
    }
    emptyChunks_.erase(coord);
}

void VoxelMeshingSystem::render(fabric::AppContext& /*ctx*/) {
    FABRIC_ZONE_SCOPED_N("voxel_meshing");
    processFrame();
}

void VoxelMeshingSystem::configureDependencies() {
    after<VoxelSimulationSystem>();
    before<ShadowRenderSystem>();
}

void VoxelMeshingSystem::onWorldBegin() {
    // Meshes generated reactively from simulation; no initialization needed.
}

void VoxelMeshingSystem::onWorldEnd() {
    clearAllMeshes();
}

void VoxelMeshingSystem::setSimulationGrid(recurse::simulation::SimulationGrid* grid) {
    simGrid_ = grid;
}

void VoxelMeshingSystem::setActivityTracker(recurse::simulation::ChunkActivityTracker* tracker) {
    activityTracker_ = tracker;
}

void VoxelMeshingSystem::processFrame() {
    if ((!simGrid_ || !activityTracker_) && simSystem_) {
        simGrid_ = &simSystem_->simulationGrid();
        activityTracker_ = &simSystem_->activityTracker();
    }

    if (!activityTracker_ || !simGrid_ || !mesher_) {
        pendingMeshCount_ = 0;
        return;
    }

    meshedThisFrame_ = 0;
    emptySkippedThisFrame_ = 0;

    // Collect chunks to mesh from both active set and unmeshed set
    std::vector<recurse::simulation::ActiveChunkEntry> activeChunks;
    {
        FABRIC_ZONE_SCOPED_N("mesh_collect_active");
        activeChunks = activityTracker_->collectActiveChunks(meshBudget_);
    }

    std::vector<fabric::ChunkCoord> toMesh;
    toMesh.reserve(static_cast<size_t>(meshBudget_));
    std::unordered_set<fabric::ChunkCoord, fabric::ChunkCoordHash> scheduled;

    for (const auto& entry : activeChunks) {
        auto coord = entry.pos;
        toMesh.push_back(coord);
        scheduled.insert(coord);
    }

    int remaining = meshBudget_ - static_cast<int>(activeChunks.size());
    if (remaining > 0 && simGrid_) {
        auto& registry = simGrid_->registry();
        auto allChunks = simGrid_->allChunks();
        for (const auto& [cx, cy, cz] : allChunks) {
            if (static_cast<int>(toMesh.size()) >= meshBudget_)
                break;
            fabric::ChunkCoord coord{cx, cy, cz};
            if (scheduled.contains(coord))
                continue;
            auto* slot = registry.find(cx, cy, cz);
            if (!slot || slot->state != recurse::simulation::ChunkSlotState::Active)
                continue;
            if (gpuMeshes_.find(coord) != gpuMeshes_.end())
                continue;
            if (emptyChunks_.find(coord) != emptyChunks_.end()) {
                ++emptySkippedThisFrame_;
                continue;
            }
            toMesh.push_back(coord);
            scheduled.insert(coord);
        }
    }

    if (toMesh.empty()) {
        pendingMeshCount_ = activityTracker_->activeChunkCount();
        return;
    }

    // Parallel CPU mesh generation
    std::vector<CPUMeshResult> results(toMesh.size());
    {
        FABRIC_ZONE_SCOPED_N("mesh_parallel_gen");
        if (scheduler_) {
            scheduler_->parallelFor(toMesh.size(), [&](size_t jobIdx, size_t /*workerIdx*/) {
                results[jobIdx] = generateMeshCPU(toMesh[jobIdx]);
            });
        } else {
            for (size_t i = 0; i < toMesh.size(); ++i)
                results[i] = generateMeshCPU(toMesh[i]);
        }
    }

    // Sequential GPU upload and bookkeeping
    {
        FABRIC_ZONE_SCOPED_N("mesh_gpu_upload");
        for (size_t i = 0; i < results.size(); ++i)
            uploadMeshResult(toMesh[i], std::move(results[i]));
    }

    // Activity tracker updates (sequential; D-34)
    for (const auto& entry : activeChunks) {
        if (activityTracker_->getState(entry.pos) != recurse::simulation::ChunkState::Active)
            activityTracker_->putToSleep(entry.pos);
    }

    pendingMeshCount_ = activityTracker_->activeChunkCount();
}

MeshingChunkContext VoxelMeshingSystem::buildMeshingContext(const fabric::ChunkCoord& coord) const {
    MeshingChunkContext ctx{};
    ctx.cx = coord.x;
    ctx.cy = coord.y;
    ctx.cz = coord.z;
    ctx.self = simGrid_->readBuffer(coord.x, coord.y, coord.z);
    ctx.selfFill = simGrid_->getChunkFillValue(coord.x, coord.y, coord.z);
    ctx.palette = simGrid_->chunkPalette(coord.x, coord.y, coord.z);

    // +X, -X, +Y, -Y, +Z, -Z
    static constexpr int K_OFFSETS[6][3] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
    for (int i = 0; i < 6; ++i) {
        int nx = coord.x + K_OFFSETS[i][0];
        int ny = coord.y + K_OFFSETS[i][1];
        int nz = coord.z + K_OFFSETS[i][2];
        ctx.neighbors[i] = simGrid_->readBuffer(nx, ny, nz);
        ctx.neighborFill[i] = simGrid_->getChunkFillValue(nx, ny, nz);
    }

    return ctx;
}

CPUMeshResult VoxelMeshingSystem::generateMeshCPU(const fabric::ChunkCoord& coord) const {
    CPUMeshResult result;

    if (!simGrid_->hasChunk(coord.x, coord.y, coord.z))
        return result;

    if (requireNeighborsForMeshing_) {
        for (int d = 0; d < 4; ++d) {
            if (!simGrid_->hasChunk(coord.x + K_HORIZONTAL_NEIGHBORS[d][0], coord.y,
                                    coord.z + K_HORIZONTAL_NEIGHBORS[d][2])) {
                result.deferred = true;
                return result;
            }
        }
    }

    auto meshCtx = buildMeshingContext(coord);

    ChunkedGrid<float> densityGrid;
    ChunkedGrid<uint16_t> materialGrid;

    const int baseX = coord.x * K_CHUNK_SIZE;
    const int baseY = coord.y * K_CHUNK_SIZE;
    const int baseZ = coord.z * K_CHUNK_SIZE;

    constexpr int K_SAMPLE_MARGIN = 1;
    for (int lz = -K_SAMPLE_MARGIN; lz <= K_CHUNK_SIZE + K_SAMPLE_MARGIN; ++lz) {
        for (int ly = -K_SAMPLE_MARGIN; ly <= K_CHUNK_SIZE + K_SAMPLE_MARGIN; ++ly) {
            for (int lx = -K_SAMPLE_MARGIN; lx <= K_CHUNK_SIZE + K_SAMPLE_MARGIN; ++lx) {
                const auto cell = meshCtx.readLocal(lx, ly, lz, simGrid_);
                const float density = (cell.materialId == recurse::simulation::material_ids::AIR) ? 0.0f : 1.0f;
                densityGrid.set(baseX + lx, baseY + ly, baseZ + lz, density);
                materialGrid.set(baseX + lx, baseY + ly, baseZ + lz, cell.materialId);
            }
        }
    }

    ChunkDensityCache densityCache;
    ChunkMaterialCache materialCache;
    densityCache.build(coord.x, coord.y, coord.z, densityGrid);
    materialCache.build(coord.x, coord.y, coord.z, materialGrid);

    auto meshData = mesher_->meshChunk(densityCache, materialCache, 0.5f, 0);
    if (meshData.empty() || meshData.indices.empty())
        return result;

    // Terrain appearance contract: full-res chunk meshes use the same
    // MaterialDef::baseColor truth as distant LOD sections. Chunk-local essence
    // remains available for simulation and debug inspection, but does not drive
    // this terrain palette.
    std::set<uint16_t> uniqueMaterials;
    for (const auto& v : meshData.vertices)
        uniqueMaterials.insert(unpackMaterialId(v.material));

    std::unordered_map<uint16_t, uint16_t> paletteLookup;
    result.palette.reserve(uniqueMaterials.size());
    for (uint16_t matId : uniqueMaterials) {
        paletteLookup[matId] = static_cast<uint16_t>(result.palette.size());
        if (materials_) {
            result.palette.push_back(materials_->terrainAppearanceColor(matId));
        } else {
            result.palette.push_back({0.0f, 0.0f, 0.0f, 0.0f});
        }
    }

    result.vertices.reserve(meshData.vertices.size());
    for (const auto& v : meshData.vertices) {
        recurse::SmoothVoxelVertex vertex = v;
        vertex.material = recurse::SmoothVoxelVertex::packShaderMaterial(paletteLookup[unpackMaterialId(v.material)]);
        result.vertices.push_back(vertex);
    }

    result.indices = std::move(meshData.indices);
    result.valid = !result.vertices.empty() && !result.indices.empty();
    return result;
}

void VoxelMeshingSystem::uploadMeshResult(const fabric::ChunkCoord& coord, CPUMeshResult&& result) {
    auto existing = gpuMeshes_.find(coord);

    if (!result.valid) {
        if (existing != gpuMeshes_.end()) {
            destroyChunkMesh(existing->second);
            gpuMeshes_.erase(existing);
        }
        // Track genuinely empty chunks (not deferred) to avoid re-processing
        if (!result.deferred && simGrid_->hasChunk(coord.x, coord.y, coord.z)) {
            bool allNeighborsExist = true;
            if (requireNeighborsForMeshing_) {
                for (int d = 0; d < 4; ++d) {
                    if (!simGrid_->hasChunk(coord.x + K_HORIZONTAL_NEIGHBORS[d][0], coord.y,
                                            coord.z + K_HORIZONTAL_NEIGHBORS[d][2])) {
                        allNeighborsExist = false;
                        break;
                    }
                }
            }
            if (allNeighborsExist)
                emptyChunks_.insert(coord);
        }
        return;
    }

    ChunkGPUMesh gpuMesh;
    gpuMesh.vertexCount = static_cast<uint32_t>(result.vertices.size());
    gpuMesh.indexCount = static_cast<uint32_t>(result.indices.size());
    gpuMesh.mesh.indexCount = gpuMesh.indexCount;
    gpuMesh.mesh.palette = std::move(result.palette);
    gpuMesh.mesh.valid = true;
    gpuMesh.valid = true;

    if (gpuUploadEnabled_) {
        gpuMesh.mesh.vbh.reset(bgfx::createVertexBuffer(
            bgfx::copy(result.vertices.data(),
                       static_cast<uint32_t>(result.vertices.size() * sizeof(recurse::SmoothVoxelVertex))),
            recurse::SmoothVoxelVertex::getVertexLayout()));
        if (!gpuMesh.mesh.vbh.isValid()) {
            gpuMesh.valid = false;
            gpuMesh.mesh.valid = false;
        }

        if (gpuMesh.valid) {
            gpuMesh.mesh.ibh.reset(bgfx::createIndexBuffer(
                bgfx::copy(result.indices.data(), static_cast<uint32_t>(result.indices.size() * sizeof(uint32_t))),
                BGFX_BUFFER_INDEX32));
            if (!gpuMesh.mesh.ibh.isValid()) {
                gpuMesh.mesh.vbh.reset();
                gpuMesh.valid = false;
                gpuMesh.mesh.valid = false;
            }
        }
    }

    if (!gpuMesh.valid)
        return;

    if (existing != gpuMeshes_.end()) {
        destroyChunkMesh(existing->second);
        existing->second = std::move(gpuMesh);
    } else {
        gpuMeshes_.emplace(coord, std::move(gpuMesh));
    }
    vertexBufferSize_ += static_cast<size_t>(gpuMesh.vertexCount);
    indexBufferSize_ += static_cast<size_t>(gpuMesh.indexCount);
    ++meshedThisFrame_;
}

void VoxelMeshingSystem::destroyChunkMesh(ChunkGPUMesh& gpuMesh) {
    if (gpuMesh.valid) {
        vertexBufferSize_ -= static_cast<size_t>(gpuMesh.vertexCount);
        indexBufferSize_ -= static_cast<size_t>(gpuMesh.indexCount);
    }
    gpuMesh.mesh.vbh.reset();
    gpuMesh.mesh.ibh.reset();
    gpuMesh.mesh.indexCount = 0;
    gpuMesh.mesh.palette.clear();
    gpuMesh.mesh.valid = false;

    gpuMesh.vertexCount = 0;
    gpuMesh.indexCount = 0;
    gpuMesh.valid = false;
}

size_t VoxelMeshingSystem::pendingMeshCount() const {
    return pendingMeshCount_;
}

size_t VoxelMeshingSystem::vertexBufferSize() const {
    return vertexBufferSize_;
}

size_t VoxelMeshingSystem::indexBufferSize() const {
    return indexBufferSize_;
}

MeshingDebugInfo VoxelMeshingSystem::debugInfo() const {
    MeshingDebugInfo info;
    info.chunksMeshedThisFrame = meshedThisFrame_;
    info.emptyChunksSkipped = emptySkippedThisFrame_;
    info.budgetRemaining = meshBudget_ - meshedThisFrame_;
    return info;
}

} // namespace recurse::systems
