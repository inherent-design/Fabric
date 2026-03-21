#include "recurse/systems/VoxelMeshingSystem.hh"
#include "fabric/world/ChunkCoordUtils.hh"

#include "fabric/core/AppContext.hh"
#include "fabric/core/SystemRegistry.hh"
#include "fabric/core/WorldLifecycle.hh"
#include "fabric/log/Log.hh"
#include "fabric/platform/ConfigManager.hh"
#include "fabric/platform/JobScheduler.hh"
#include "fabric/utils/Profiler.hh"
#include "fabric/world/ChunkedGrid.hh"
#include "recurse/simulation/CellAccessors.hh"
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

#include <algorithm>
#include <bgfx/bgfx.h>
#include <cmath>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace recurse::systems {

using fabric::ChunkedGrid;
using fabric::K_HORIZONTAL_NEIGHBORS;
using recurse::simulation::K_CHUNK_SIZE;

namespace {

constexpr std::string_view K_NEAR_CHUNK_MESHER_CONFIG_KEY = "voxel_meshing.near_chunk_mesher";

constexpr int K_FACE_AXIS_COUNT = 3;

bool isSolidVoxel(const recurse::simulation::VoxelCell& cell) {
    return recurse::simulation::isOccupied(cell);
}

uint16_t unpackMaterialId(uint32_t packedMaterial) {
    return static_cast<uint16_t>(packedMaterial & 0xFFFFu);
}

uint8_t unpackAO(uint32_t packedMaterial) {
    return static_cast<uint8_t>((packedMaterial >> 16) & 0xFFu);
}

uint8_t unpackFlags(uint32_t packedMaterial) {
    return static_cast<uint8_t>((packedMaterial >> 24) & 0xFFu);
}

uint8_t normalizeShaderAO(uint32_t packedMaterial) {
    const uint8_t ao = unpackAO(packedMaterial);
    if (ao <= 15)
        return ao;
    return recurse::SmoothVoxelVertex::K_SHADER_DEFAULT_AO;
}

const char* nearChunkMesherName(VoxelMeshingSystem::NearChunkMesher mesher) {
    switch (mesher) {
        case VoxelMeshingSystem::NearChunkMesher::SnapMC:
            return "SnapMC";
        case VoxelMeshingSystem::NearChunkMesher::Greedy:
            return "Greedy";
    }
    return "Unknown";
}

std::optional<VoxelMeshingSystem::NearChunkMesher> parseNearChunkMesher(std::string_view value) {
    if (value == "snapmc" || value == "snap_mc")
        return VoxelMeshingSystem::NearChunkMesher::SnapMC;
    if (value == "greedy")
        return VoxelMeshingSystem::NearChunkMesher::Greedy;
    return std::nullopt;
}

recurse::simulation::VoxelCell readGreedyCell(const MeshingChunkContext& ctx, int axis, int slice, int row, int col,
                                              int normalOffset, const recurse::simulation::SimulationGrid* fallback) {
    switch (axis) {
        case 0:
            return ctx.readLocal(slice + normalOffset, row, col, fallback);
        case 1:
            return ctx.readLocal(row, slice + normalOffset, col, fallback);
        default:
            return ctx.readLocal(row, col, slice + normalOffset, fallback);
    }
}

void appendGreedyVertex(recurse::SmoothChunkMeshData& output, float px, float py, float pz, float nx, float ny,
                        float nz, uint16_t materialId) {
    recurse::SmoothVoxelVertex vertex{};
    vertex.px = px;
    vertex.py = py;
    vertex.pz = pz;
    vertex.nx = nx;
    vertex.ny = ny;
    vertex.nz = nz;
    vertex.material =
        recurse::SmoothVoxelVertex::packMaterial(materialId, recurse::SmoothVoxelVertex::K_SHADER_DEFAULT_AO);
    vertex.padding = 0;
    output.vertices.push_back(vertex);
}

void emitGreedyQuad(recurse::SmoothChunkMeshData& output, int axis, bool positive, int slice, int row, int col,
                    int width, int height, uint16_t materialId) {
    const float plane = static_cast<float>(positive ? slice + 1 : slice);
    const float u0 = static_cast<float>(row);
    const float u1 = static_cast<float>(row + height);
    const float v0 = static_cast<float>(col);
    const float v1 = static_cast<float>(col + width);

    float nx = 0.0f;
    float ny = 0.0f;
    float nz = 0.0f;
    if (axis == 0) {
        nx = positive ? 1.0f : -1.0f;
        if (positive) {
            appendGreedyVertex(output, plane, u0, v0, nx, ny, nz, materialId);
            appendGreedyVertex(output, plane, u1, v0, nx, ny, nz, materialId);
            appendGreedyVertex(output, plane, u1, v1, nx, ny, nz, materialId);
            appendGreedyVertex(output, plane, u0, v1, nx, ny, nz, materialId);
        } else {
            appendGreedyVertex(output, plane, u0, v0, nx, ny, nz, materialId);
            appendGreedyVertex(output, plane, u0, v1, nx, ny, nz, materialId);
            appendGreedyVertex(output, plane, u1, v1, nx, ny, nz, materialId);
            appendGreedyVertex(output, plane, u1, v0, nx, ny, nz, materialId);
        }
    } else if (axis == 1) {
        ny = positive ? 1.0f : -1.0f;
        if (positive) {
            appendGreedyVertex(output, u0, plane, v0, nx, ny, nz, materialId);
            appendGreedyVertex(output, u0, plane, v1, nx, ny, nz, materialId);
            appendGreedyVertex(output, u1, plane, v1, nx, ny, nz, materialId);
            appendGreedyVertex(output, u1, plane, v0, nx, ny, nz, materialId);
        } else {
            appendGreedyVertex(output, u0, plane, v0, nx, ny, nz, materialId);
            appendGreedyVertex(output, u1, plane, v0, nx, ny, nz, materialId);
            appendGreedyVertex(output, u1, plane, v1, nx, ny, nz, materialId);
            appendGreedyVertex(output, u0, plane, v1, nx, ny, nz, materialId);
        }
    } else {
        nz = positive ? 1.0f : -1.0f;
        if (positive) {
            appendGreedyVertex(output, u0, v0, plane, nx, ny, nz, materialId);
            appendGreedyVertex(output, u1, v0, plane, nx, ny, nz, materialId);
            appendGreedyVertex(output, u1, v1, plane, nx, ny, nz, materialId);
            appendGreedyVertex(output, u0, v1, plane, nx, ny, nz, materialId);
        } else {
            appendGreedyVertex(output, u0, v0, plane, nx, ny, nz, materialId);
            appendGreedyVertex(output, u0, v1, plane, nx, ny, nz, materialId);
            appendGreedyVertex(output, u1, v1, plane, nx, ny, nz, materialId);
            appendGreedyVertex(output, u1, v0, plane, nx, ny, nz, materialId);
        }
    }

    const uint32_t base = static_cast<uint32_t>(output.vertices.size() - 4);
    output.indices.insert(output.indices.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
}

recurse::SmoothChunkMeshData buildGreedyMesh(const MeshingChunkContext& ctx,
                                             const recurse::simulation::SimulationGrid* fallback) {
    recurse::SmoothChunkMeshData output;
    output.vertices.reserve(1024);
    output.indices.reserve(1536);

    std::array<uint16_t, K_CHUNK_SIZE * K_CHUNK_SIZE> mask{};
    std::array<bool, K_CHUNK_SIZE * K_CHUNK_SIZE> consumed{};

    for (int axis = 0; axis < K_FACE_AXIS_COUNT; ++axis) {
        for (bool positive : {false, true}) {
            for (int slice = 0; slice < K_CHUNK_SIZE; ++slice) {
                mask.fill(recurse::simulation::material_ids::AIR);
                consumed.fill(false);

                for (int row = 0; row < K_CHUNK_SIZE; ++row) {
                    for (int col = 0; col < K_CHUNK_SIZE; ++col) {
                        const auto cell = readGreedyCell(ctx, axis, slice, row, col, 0, fallback);
                        if (!isSolidVoxel(cell))
                            continue;

                        const auto neighbor = readGreedyCell(ctx, axis, slice, row, col, positive ? 1 : -1, fallback);
                        if (isSolidVoxel(neighbor))
                            continue;

                        mask[static_cast<size_t>(row * K_CHUNK_SIZE + col)] = cell.materialId;
                    }
                }

                for (int row = 0; row < K_CHUNK_SIZE; ++row) {
                    for (int col = 0; col < K_CHUNK_SIZE; ++col) {
                        const size_t startIdx = static_cast<size_t>(row * K_CHUNK_SIZE + col);
                        const uint16_t materialId = mask[startIdx];
                        if (materialId == recurse::simulation::material_ids::AIR || consumed[startIdx])
                            continue;

                        int width = 1;
                        while (col + width < K_CHUNK_SIZE) {
                            const size_t idx = static_cast<size_t>(row * K_CHUNK_SIZE + col + width);
                            if (mask[idx] != materialId || consumed[idx])
                                break;
                            ++width;
                        }

                        int height = 1;
                        bool canGrow = true;
                        while (row + height < K_CHUNK_SIZE && canGrow) {
                            for (int offset = 0; offset < width; ++offset) {
                                const size_t idx = static_cast<size_t>((row + height) * K_CHUNK_SIZE + col + offset);
                                if (mask[idx] != materialId || consumed[idx]) {
                                    canGrow = false;
                                    break;
                                }
                            }
                            if (canGrow)
                                ++height;
                        }

                        for (int dy = 0; dy < height; ++dy) {
                            for (int dx = 0; dx < width; ++dx) {
                                consumed[static_cast<size_t>((row + dy) * K_CHUNK_SIZE + col + dx)] = true;
                            }
                        }

                        emitGreedyQuad(output, axis, positive, slice, row, col, width, height, materialId);
                    }
                }
            }
        }
    }

    return output;
}

uint8_t greedyNormalIndex(const recurse::SmoothVoxelVertex& vertex) {
    const float absX = std::fabs(vertex.nx);
    const float absY = std::fabs(vertex.ny);
    const float absZ = std::fabs(vertex.nz);
    if (absX >= absY && absX >= absZ)
        return vertex.nx >= 0.0f ? 0u : 1u;
    if (absY >= absX && absY >= absZ)
        return vertex.ny >= 0.0f ? 2u : 3u;
    return vertex.nz >= 0.0f ? 4u : 5u;
}

recurse::VoxelVertex packGreedyVoxelVertex(const recurse::SmoothVoxelVertex& vertex) {
    const auto quantize = [](float value) {
        return static_cast<uint8_t>(std::clamp(static_cast<int>(std::lround(value)), 0, K_CHUNK_SIZE));
    };

    const uint8_t ao = std::min<uint8_t>(vertex.getAO(), 3u);
    return recurse::VoxelVertex::pack(quantize(vertex.px), quantize(vertex.py), quantize(vertex.pz),
                                      greedyNormalIndex(vertex), ao, vertex.getMaterialId());
}

} // namespace

VoxelMeshingSystem::VoxelMeshingSystem() = default;

VoxelMeshingSystem::~VoxelMeshingSystem() {
    for (auto& [_, gpuMesh] : gpuMeshes_)
        destroyChunkMesh(gpuMesh);
}

void VoxelMeshingSystem::setNearChunkMesher(NearChunkMesher mesher) {
    nearChunkMesher_ = mesher;
    if (nearChunkMesher_ == NearChunkMesher::SnapMC && !snapMcMesher_)
        snapMcMesher_ = std::make_unique<SnapMCMesher>();
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
    const auto configuredNearChunkMesher =
        ctx.configManager.get<std::string>(K_NEAR_CHUNK_MESHER_CONFIG_KEY, std::string{"greedy"});
    if (const auto parsed = parseNearChunkMesher(configuredNearChunkMesher)) {
        setNearChunkMesher(*parsed);
    } else {
        setNearChunkMesher(NearChunkMesher::Greedy);
        FABRIC_LOG_WARN("VoxelMeshingSystem: unknown {}='{}'; falling back to {}", K_NEAR_CHUNK_MESHER_CONFIG_KEY,
                        configuredNearChunkMesher, nearChunkMesherName(nearChunkMesher_));
    }

    if (nearChunkMesher_ == NearChunkMesher::SnapMC) {
        FABRIC_LOG_INFO("VoxelMeshingSystem: {}='{}' selects experimental SnapMC comparison path",
                        K_NEAR_CHUNK_MESHER_CONFIG_KEY, configuredNearChunkMesher);
    }

    gpuUploadEnabled_ = (ctx.renderCaps != nullptr);

    FABRIC_LOG_INFO(
        "VoxelMeshingSystem initialized (budget={}, simBound={}, trackerBound={}, nearMesher={}, gpuUpload={})",
        meshBudget_, simGrid_ != nullptr, activityTracker_ != nullptr, nearChunkMesherName(nearChunkMesher_),
        gpuUploadEnabled_);
}

void VoxelMeshingSystem::doShutdown() {
    clearAllMeshes();
    snapMcMesher_.reset();
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
    vertexBufferBytes_ = 0;
    indexBufferBytes_ = 0;
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

    if (!activityTracker_ || !simGrid_ || (nearChunkMesher_ == NearChunkMesher::SnapMC && !snapMcMesher_)) {
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
            scheduler_->parallelFor(toMesh.size(), "mesh_parallel_gen", [&](size_t jobIdx, size_t /*workerIdx*/) {
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

    recurse::SmoothChunkMeshData meshData;
    switch (nearChunkMesher_) {
        case NearChunkMesher::Greedy:
            meshData = buildGreedyMesh(meshCtx, simGrid_);
            break;
        case NearChunkMesher::SnapMC: {
            result.meshFormat = recurse::ChunkMesh::VertexFormat::Smooth;
            if (!snapMcMesher_)
                return result;

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
                        const float density = recurse::simulation::isEmpty(cell) ? 0.0f : 1.0f;
                        densityGrid.set(baseX + lx, baseY + ly, baseZ + lz, density);
                        materialGrid.set(baseX + lx, baseY + ly, baseZ + lz, cell.materialId);
                    }
                }
            }

            ChunkDensityCache densityCache;
            ChunkMaterialCache materialCache;
            densityCache.build(coord.x, coord.y, coord.z, densityGrid);
            materialCache.build(coord.x, coord.y, coord.z, materialGrid);

            meshData = snapMcMesher_->meshChunk(densityCache, materialCache, 0.5f, 0);
            break;
        }
    }

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
        vertex.material = recurse::SmoothVoxelVertex::packShaderMaterial(
            paletteLookup[unpackMaterialId(v.material)], normalizeShaderAO(v.material), unpackFlags(v.material));
        result.vertices.push_back(vertex);
    }

    if (nearChunkMesher_ == NearChunkMesher::Greedy) {
        result.meshFormat = recurse::ChunkMesh::VertexFormat::Voxel;
        result.voxelVertices.reserve(result.vertices.size());
        for (const auto& vertex : result.vertices)
            result.voxelVertices.push_back(packGreedyVoxelVertex(vertex));
    }

    result.indices = std::move(meshData.indices);
    result.valid = !result.vertices.empty() && !result.indices.empty() &&
                   (result.meshFormat != recurse::ChunkMesh::VertexFormat::Voxel || !result.voxelVertices.empty());
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
    const bool voxelFormat = result.meshFormat == recurse::ChunkMesh::VertexFormat::Voxel;
    gpuMesh.vertexCount = static_cast<uint32_t>(voxelFormat ? result.voxelVertices.size() : result.vertices.size());
    gpuMesh.indexCount = static_cast<uint32_t>(result.indices.size());
    gpuMesh.mesh.indexCount = gpuMesh.indexCount;
    gpuMesh.mesh.palette = std::move(result.palette);
    gpuMesh.mesh.vertexFormat = result.meshFormat;
    gpuMesh.mesh.vertexStrideBytes = recurse::ChunkMesh::vertexStrideForFormat(result.meshFormat);
    gpuMesh.mesh.valid = true;
    gpuMesh.vertexBytes = static_cast<size_t>(gpuMesh.vertexCount) * gpuMesh.mesh.vertexStrideBytes;
    gpuMesh.indexBytes = static_cast<size_t>(gpuMesh.indexCount) * sizeof(uint32_t);
    gpuMesh.valid = true;

    if (gpuUploadEnabled_) {
        const void* vertexData = voxelFormat ? static_cast<const void*>(result.voxelVertices.data())
                                             : static_cast<const void*>(result.vertices.data());
        const bgfx::VertexLayout& layout =
            voxelFormat ? recurse::VoxelVertex::getVertexLayout() : recurse::SmoothVoxelVertex::getVertexLayout();
        gpuMesh.mesh.vbh.reset(
            bgfx::createVertexBuffer(bgfx::copy(vertexData, static_cast<uint32_t>(gpuMesh.vertexBytes)), layout));
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
    vertexBufferBytes_ += gpuMesh.vertexBytes;
    indexBufferBytes_ += gpuMesh.indexBytes;
    ++meshedThisFrame_;
}

void VoxelMeshingSystem::destroyChunkMesh(ChunkGPUMesh& gpuMesh) {
    if (gpuMesh.valid) {
        vertexBufferSize_ -= static_cast<size_t>(gpuMesh.vertexCount);
        indexBufferSize_ -= static_cast<size_t>(gpuMesh.indexCount);
        vertexBufferBytes_ -= gpuMesh.vertexBytes;
        indexBufferBytes_ -= gpuMesh.indexBytes;
    }
    gpuMesh.mesh.vbh.reset();
    gpuMesh.mesh.ibh.reset();
    gpuMesh.mesh.indexCount = 0;
    gpuMesh.mesh.palette.clear();
    gpuMesh.mesh.vertexFormat = recurse::ChunkMesh::VertexFormat::Voxel;
    gpuMesh.mesh.vertexStrideBytes = recurse::ChunkMesh::vertexStrideForFormat(recurse::ChunkMesh::VertexFormat::Voxel);
    gpuMesh.mesh.valid = false;

    gpuMesh.vertexCount = 0;
    gpuMesh.indexCount = 0;
    gpuMesh.vertexBytes = 0;
    gpuMesh.indexBytes = 0;
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

size_t VoxelMeshingSystem::vertexBufferBytes() const {
    return vertexBufferBytes_;
}

size_t VoxelMeshingSystem::indexBufferBytes() const {
    return indexBufferBytes_;
}

MeshingDebugInfo VoxelMeshingSystem::debugInfo() const {
    MeshingDebugInfo info;
    info.chunksMeshedThisFrame = meshedThisFrame_;
    info.emptyChunksSkipped = emptySkippedThisFrame_;
    info.budgetRemaining = meshBudget_ - meshedThisFrame_;
    return info;
}

} // namespace recurse::systems
