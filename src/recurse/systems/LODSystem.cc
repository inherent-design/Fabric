#include "recurse/systems/LODSystem.hh"

#include "fabric/core/AppContext.hh"
#include "fabric/core/Spatial.hh"
#include "fabric/core/SystemRegistry.hh"
#include "fabric/core/WorldLifecycle.hh"
#include "fabric/log/Log.hh"
#include "fabric/platform/ConfigManager.hh"
#include "fabric/platform/JobScheduler.hh"
#include "fabric/render/Camera.hh"
#include "fabric/render/SceneView.hh"
#include "fabric/utils/Profiler.hh"
#include "fabric/world/ChunkedGrid.hh"
#include "recurse/render/LODGrid.hh"
#include "recurse/render/LODMeshManager.hh"
#include "recurse/simulation/MaterialRegistry.hh"
#include "recurse/simulation/SimulationGrid.hh"
#include "recurse/simulation/VoxelMaterial.hh"
#include "recurse/systems/TerrainSystem.hh"
#include "recurse/systems/VoxelRenderSystem.hh"
#include "recurse/systems/VoxelSimulationSystem.hh"
#include "recurse/world/VoxelVertex.hh"
#include "recurse/world/WorldGenerator.hh"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>
#include <vector>

namespace recurse::systems {

namespace {

constexpr size_t K_INLINE_LOD_FILL_TASK_THRESHOLD = 2;
constexpr size_t K_COARSENED_LOD_FILL_TASK_THRESHOLD = 4;
constexpr size_t K_COARSENED_LOD_FILL_JOB_COUNT = 2;

struct LODGenTask {
    LODSection* section;
    int cx, cy, cz;
    bool useWorldGen;
};

std::pair<size_t, size_t> partitionRange(size_t taskCount, size_t jobCount, size_t jobIdx) {
    const size_t start = (jobIdx * taskCount) / jobCount;
    const size_t end = ((jobIdx + 1) * taskCount) / jobCount;
    return {start, end};
}

void fillLODSectionTask(const LODGenTask& task, recurse::simulation::SimulationGrid* grid, WorldGenerator* gen) {
    auto* section = task.section;
    section->origin = LODGrid::sectionOrigin(0, task.cx, task.cy, task.cz);
    section->palette.clear();
    section->palette.push_back(simulation::material_ids::AIR);
    section->blockIndices.assign(LODSection::K_VOLUME, 0);

    for (int lz = 0; lz < LODSection::K_SIZE; ++lz) {
        for (int ly = 0; ly < LODSection::K_SIZE; ++ly) {
            for (int lx = 0; lx < LODSection::K_SIZE; ++lx) {
                int wx = section->origin.x + lx;
                int wy = section->origin.y + ly;
                int wz = section->origin.z + lz;

                uint16_t matId =
                    task.useWorldGen ? gen->sampleMaterial(wx, wy, wz) : grid->readCell(wx, wy, wz).materialId;

                uint16_t palIdx = 0;
                auto it = std::find(section->palette.begin(), section->palette.end(), matId);
                if (it != section->palette.end()) {
                    palIdx = static_cast<uint16_t>(std::distance(section->palette.begin(), it));
                } else {
                    palIdx = static_cast<uint16_t>(section->palette.size());
                    section->palette.push_back(matId);
                }
                section->set(lx, ly, lz, palIdx);
            }
        }
    }

    section->dirty = true;
}

} // namespace

LODSystem::LODSystem() : grid_(std::make_unique<LODGrid>()) {}

LODSystem::~LODSystem() = default;

void LODSystem::doInit(fabric::AppContext& ctx) {
    if (auto* wl = ctx.worldLifecycle) {
        wl->registerParticipant([this]() { onWorldBegin(); }, [this]() { onWorldEnd(); });
    }
    uploadBudget_ = ctx.configManager.get<int>("lod.upload_budget", 50);
    genBudget_ = ctx.configManager.get<int>("lod.gen_budget", 16);
    maxLODLevel_ = ctx.configManager.get<int>("lod.max_level", 6);
    baseRadius_ = ctx.configManager.get<float>("lod.base_radius", 10.0f);

    auto* voxelSim = ctx.systemRegistry.get<VoxelSimulationSystem>();
    if (voxelSim) {
        setSimulationGrid(&voxelSim->simulationGrid());
        setMaterialRegistry(&voxelSim->materials());
        scheduler_ = &voxelSim->scheduler();

        // Queue any chunks that were generated before LODSystem was enabled
        auto allChunks = simGrid_->allChunks();
        for (const auto& [cx, cy, cz] : allChunks) {
            pendingChunks_.emplace_back(cx, cy, cz);
        }
        FABRIC_LOG_INFO("LODSystem initialized: {} existing chunks queued", allChunks.size());
    } else {
        FABRIC_LOG_INFO("LODSystem initialized (no VoxelSimulationSystem available)");
    }

    auto* renderSystem = ctx.systemRegistry.get<VoxelRenderSystem>();
    if (renderSystem)
        voxelRenderer_ = &renderSystem->voxelRenderer();

    terrain_ = ctx.systemRegistry.get<TerrainSystem>();
}

void LODSystem::doShutdown() {
    gpuSections_.clear();
    visibleSections_.clear();
    pendingChunks_.clear();
    pendingDirectChunks_.clear();
    clearFullResCoverage();
    simGrid_ = nullptr;
    materials_ = nullptr;
    scheduler_ = nullptr;
    voxelRenderer_ = nullptr;
    terrain_ = nullptr;

    FABRIC_LOG_INFO("LODSystem shut down");
}

void LODSystem::fixedUpdate(fabric::AppContext& /*ctx*/, float /*fixedDt*/) {
    // LODSystem is registered in PreRender (dispatches render(), not fixedUpdate()).
    // Generation logic lives in render() alongside upload and submission.
}

void LODSystem::render(fabric::AppContext& ctx) {
    if (!grid_)
        return;

    // Generate pending LOD sections (budget-capped parallel fill).
    if (scheduler_) {
        std::vector<LODGenTask> tasks;
        tasks.reserve(static_cast<size_t>(genBudget_));

        while (simGrid_ && !pendingChunks_.empty() && static_cast<int>(tasks.size()) < genBudget_) {
            auto [cx, cy, cz] = pendingChunks_.front();
            pendingChunks_.pop_front();
            auto* section = grid_->getOrCreate(0, cx, cy, cz);
            if (!section)
                continue;
            tasks.push_back({section, cx, cy, cz, false});
        }

        while (terrain_ && !pendingDirectChunks_.empty() && static_cast<int>(tasks.size()) < genBudget_) {
            auto [cx, cy, cz] = pendingDirectChunks_.front();
            pendingDirectChunks_.pop_front();
            auto* section = grid_->getOrCreate(0, cx, cy, cz);
            if (!section)
                continue;
            tasks.push_back({section, cx, cy, cz, true});
        }

        if (!tasks.empty()) {
            FABRIC_ZONE_SCOPED_N("lod_section_fill");
            FABRIC_ZONE_VALUE(static_cast<int64_t>(tasks.size()));
            auto* grid = simGrid_;
            auto* gen = terrain_ ? &terrain_->worldGenerator() : nullptr;
            if (tasks.size() <= K_INLINE_LOD_FILL_TASK_THRESHOLD) {
                FABRIC_ZONE_SCOPED_N("lod_section_fill_inline");
                FABRIC_ZONE_VALUE(static_cast<int64_t>(tasks.size()));
                for (const auto& task : tasks)
                    fillLODSectionTask(task, grid, gen);
            } else if (tasks.size() <= K_COARSENED_LOD_FILL_TASK_THRESHOLD) {
                const size_t jobCount = std::min(tasks.size(), K_COARSENED_LOD_FILL_JOB_COUNT);
                scheduler_->parallelFor(
                    jobCount, "lod_section_fill",
                    [&tasks, taskCount = tasks.size(), jobCount, grid, gen](size_t jobIdx, size_t /*workerIdx*/) {
                        auto [start, end] = partitionRange(taskCount, jobCount, jobIdx);
                        for (size_t taskIdx = start; taskIdx < end; ++taskIdx)
                            fillLODSectionTask(tasks[taskIdx], grid, gen);
                    });
            } else {
                scheduler_->parallelFor(tasks.size(), "lod_section_fill",
                                        [&tasks, grid, gen](size_t idx, size_t /*workerIdx*/) {
                                            fillLODSectionTask(tasks[idx], grid, gen);
                                        });
            }
        }
    }

    // Process dirty sections with mesh generation and GPU upload
    int uploaded = 0;
    grid_->forEach([this, &uploaded](LODSection& section) {
        if (!section.dirty) {
            return;
        }
        if (uploaded >= uploadBudget_) {
            return; // Respect per-frame budget
        }

        auto key = LODGrid::keyForSection(section);

        // Skip empty sections
        bool hasSolid = false;
        for (uint16_t idx : section.blockIndices) {
            if (section.materialOf(idx) != 0) {
                hasSolid = true;
                break;
            }
        }
        if (!hasSolid) {
            releaseGPUSection(key);
            section.dirty = false;
            int sx = LODGrid::sectionCoordFromOrigin(section.level, section.origin.x);
            int sy = LODGrid::sectionCoordFromOrigin(section.level, section.origin.y);
            int sz = LODGrid::sectionCoordFromOrigin(section.level, section.origin.z);
            grid_->tryBuildParent(section.level, sx, sy, sz);
            return;
        }

        // Mesh section if meshManager_ is available
        if (meshManager_) {
            auto mesh = meshManager_->meshSection(section);
            if (!mesh.empty()) {
                uploadSection(key, mesh);
                ++uploaded;
                FABRIC_LOG_TRACE("LODSystem: Uploaded section level={} ({},{},{}) verts={}", section.level, key.x(),
                                 key.y(), key.z(), mesh.vertices.size());
            } else {
                releaseGPUSection(key);
            }
        }

        section.dirty = false;

        // Try to build parent LOD section
        int sx = LODGrid::sectionCoordFromOrigin(section.level, section.origin.x);
        int sy = LODGrid::sectionCoordFromOrigin(section.level, section.origin.y);
        int sz = LODGrid::sectionCoordFromOrigin(section.level, section.origin.z);
        grid_->tryBuildParent(section.level, sx, sy, sz);
    });

    // Submit visible LOD sections
    if (voxelRenderer_ && grid_) {
        auto* camera = ctx.camera;
        if (camera) {
            selectVisibleSections(*camera);

            if (!visibleSections_.empty()) {
                std::vector<recurse::ChunkRenderInfo> lodBatch;
                lodBatch.reserve(visibleSections_.size());

                for (const auto& vis : visibleSections_) {
                    auto it = gpuSections_.find(vis.key.value);
                    if (it == gpuSections_.end() || !it->second.resident)
                        continue;

                    auto worldOrigin = fabric::Vector3<double, fabric::Space::World>(
                        static_cast<double>(vis.section->origin.x), static_cast<double>(vis.section->origin.y),
                        static_cast<double>(vis.section->origin.z));
                    auto relOrigin = camera->cameraRelative(worldOrigin);

                    lodBatch.push_back(recurse::ChunkRenderInfo{
                        .mesh = &it->second.mesh,
                        .offsetX = relOrigin.x,
                        .offsetY = relOrigin.y,
                        .offsetZ = relOrigin.z,
                        .sortKey = vis.key.value,
                    });
                }

                if (!lodBatch.empty()) {
                    voxelRenderer_->renderBatch(ctx.sceneView->geometryViewId(), lodBatch.data(),
                                                static_cast<uint32_t>(lodBatch.size()));
                }
            }
        }
    }
}

void LODSystem::onWorldBegin() {
    // LOD generation is triggered by chunk streaming; no initialization needed.
}

void LODSystem::onWorldEnd() {
    gpuSections_.clear();
    visibleSections_.clear();
    pendingChunks_.clear();
    pendingDirectChunks_.clear();
    clearFullResCoverage();
    if (grid_)
        grid_->clear();
}

void LODSystem::configureDependencies() {
    after<VoxelSimulationSystem>();
    after<VoxelRenderSystem>();
}

void LODSystem::setMaterialRegistry(const recurse::simulation::MaterialRegistry* materials) {
    materials_ = materials;
    if (materials_ && grid_) {
        meshManager_ = std::make_unique<LODMeshManager>(*grid_, *materials_);
        FABRIC_LOG_INFO("LODSystem LODMeshManager created with MaterialRegistry");
    }
}

void LODSystem::onChunkReady(int cx, int cy, int cz) {
    pendingChunks_.emplace_back(cx, cy, cz);
}

void LODSystem::onChunkRemoved(int cx, int cy, int cz) {
    // Remove pending entries if not yet processed
    auto matchCoord = [cx, cy, cz](const auto& t) {
        auto [px, py, pz] = t;
        return px == cx && py == cy && pz == cz;
    };
    std::erase_if(pendingChunks_, matchCoord);
    std::erase_if(pendingDirectChunks_, matchCoord);

    // Release GPU resources (VRAM) but keep LODGrid section data (RAM).
    // Section data persists so parent LOD levels remain valid and the
    // section can be re-uploaded cheaply if the chunk reloads.
    auto key = LODSectionKey::make(0, cx, cy, cz);
    releaseGPUSection(key);
}

void LODSystem::requestDirectLOD(int cx, int cy, int cz) {
    pendingDirectChunks_.emplace_back(cx, cy, cz);
}

void LODSystem::removeSectionFully(int cx, int cy, int cz) {
    auto matchCoord = [cx, cy, cz](const auto& t) {
        auto [px, py, pz] = t;
        return px == cx && py == cy && pz == cz;
    };
    std::erase_if(pendingChunks_, matchCoord);
    std::erase_if(pendingDirectChunks_, matchCoord);

    auto key = LODSectionKey::make(0, cx, cy, cz);
    releaseGPUSection(key);
    if (grid_)
        grid_->remove(key);
}

void LODSystem::selectVisibleSections(const fabric::Camera& camera) {
    visibleSections_.clear();
    fullResRejectedSections_ = 0;

    grid_->forEach([this, &camera](const LODSection& section) {
        // Compute world-space center of section
        int scale = 1 << section.level;
        float worldSize = static_cast<float>(LODSection::K_SIZE * scale);
        Vec3f center{static_cast<float>(section.origin.x) + worldSize * 0.5f,
                     static_cast<float>(section.origin.y) + worldSize * 0.5f,
                     static_cast<float>(section.origin.z) + worldSize * 0.5f};

        auto snapshot = inspectTerrainOwnership(section, camera);

        auto camPosRaw = camera.getPosition();
        Vec3f camPos{camPosRaw.x, camPosRaw.y, camPosRaw.z};

        float dx = center.x - camPos.x;
        float dy = center.y - camPos.y;
        float dz = center.z - camPos.z;
        float distance = std::sqrt(dx * dx + dy * dy + dz * dz);

        // Only include sections at the desired LOD level
        if (section.level != snapshot.desiredLevel) {
            return;
        }

        if (snapshot.hiddenByFullRes) {
            ++fullResRejectedSections_;
            return;
        }

        // Check if section has GPU mesh
        auto key = LODGrid::keyForSection(section);
        auto it = gpuSections_.find(key.value);
        if (it == gpuSections_.end() || !it->second.resident) {
            return;
        }

        VisibleSection vis;
        vis.section = &section;
        vis.distance = distance;
        vis.key = key;
        visibleSections_.push_back(vis);
    });

    // Sort by distance (near to far)
    std::sort(visibleSections_.begin(), visibleSections_.end(),
              [](const VisibleSection& a, const VisibleSection& b) { return a.distance < b.distance; });
}

void LODSystem::uploadSection(LODSectionKey key, const recurse::LODMeshManager::MeshResult& mesh) {
    auto it = gpuSections_.find(key.value);
    if (it == gpuSections_.end()) {
        it = gpuSections_.emplace(key.value, GPUSection{}).first;
    }
    auto& gpu = it->second;

    // Create vertex buffer (copy: mesh data is temporary); reset destroys old handle
    bgfx::VertexLayout layout = VoxelVertex::getVertexLayout();
    gpu.mesh.vbh.reset(bgfx::createVertexBuffer(
        bgfx::copy(mesh.vertices.data(), static_cast<uint32_t>(mesh.vertices.size() * sizeof(VoxelVertex))), layout));

    // Create index buffer (copy: mesh data is temporary); reset destroys old handle
    gpu.mesh.ibh.reset(bgfx::createIndexBuffer(
        bgfx::copy(mesh.indices.data(), static_cast<uint32_t>(mesh.indices.size() * sizeof(uint32_t))),
        BGFX_BUFFER_INDEX32));

    gpu.vertexCount = static_cast<uint32_t>(mesh.vertices.size());
    gpu.mesh.indexCount = static_cast<uint32_t>(mesh.indices.size());
    gpu.mesh.palette = mesh.palette;
    gpu.mesh.vertexFormat = recurse::ChunkMesh::VertexFormat::Voxel;
    gpu.mesh.vertexStrideBytes = sizeof(VoxelVertex);
    gpu.mesh.modelScale = static_cast<float>(LODGrid::sectionScale(key.level()));
    gpu.mesh.valid = true;
    gpu.resident = true;
}

void LODSystem::releaseGPUSection(LODSectionKey key) {
    gpuSections_.erase(key.value);
}

void LODSystem::setFullResCoverage(int centerCX, int centerCY, int centerCZ, int radius) {
    fullResCoverage_ = ChunkCoverageBox{.minCX = centerCX - radius,
                                        .minCY = centerCY - radius,
                                        .minCZ = centerCZ - radius,
                                        .maxCX = centerCX + radius,
                                        .maxCY = centerCY + radius,
                                        .maxCZ = centerCZ + radius};
    hasFullResCoverage_ = true;
}

void LODSystem::clearFullResCoverage() {
    fullResCoverage_ = {};
    hasFullResCoverage_ = false;
    fullResRejectedSections_ = 0;
}

TerrainOwnershipSnapshot LODSystem::inspectTerrainOwnership(const LODSection& section,
                                                            const fabric::Camera& camera) const {
    TerrainOwnershipSnapshot snapshot;
    snapshot.sectionCoverage = LODGrid::sectionChunkCoverage(section);
    if (hasFullResCoverage_) {
        snapshot.fullResCoverage = fullResCoverage_;
        snapshot.hiddenByFullRes = snapshot.sectionCoverage.intersects(fullResCoverage_);
    }

    auto camPosRaw = camera.getPosition();
    Vec3f camPos{camPosRaw.x, camPosRaw.y, camPosRaw.z};

    int scale = 1 << section.level;
    float worldSize = static_cast<float>(LODSection::K_SIZE * scale);
    Vec3f center{static_cast<float>(section.origin.x) + worldSize * 0.5f,
                 static_cast<float>(section.origin.y) + worldSize * 0.5f,
                 static_cast<float>(section.origin.z) + worldSize * 0.5f};

    float dx = center.x - camPos.x;
    float dy = center.y - camPos.y;
    float dz = center.z - camPos.z;
    float distance = std::sqrt(dx * dx + dy * dy + dz * dz);

    int desiredLevel = 0;
    float threshold = baseRadius_ * LODSection::K_SIZE;
    while (distance > threshold && desiredLevel < maxLODLevel_) {
        threshold *= 2.0f;
        ++desiredLevel;
    }
    snapshot.desiredLevel = desiredLevel;
    return snapshot;
}

LODDebugInfo LODSystem::debugInfo() const {
    LODDebugInfo info;
    info.pendingSections = static_cast<int>(pendingChunks_.size() + pendingDirectChunks_.size());
    info.gpuResidentSections = static_cast<int>(gpuSections_.size());
    info.visibleSections = static_cast<int>(visibleSections_.size());
    info.fullResRejectedSections = fullResRejectedSections_;
    if (hasFullResCoverage_) {
        info.fullResCenterCX = (fullResCoverage_.minCX + fullResCoverage_.maxCX) / 2;
        info.fullResCenterCY = (fullResCoverage_.minCY + fullResCoverage_.maxCY) / 2;
        info.fullResCenterCZ = (fullResCoverage_.minCZ + fullResCoverage_.maxCZ) / 2;
        info.fullResRadius = (fullResCoverage_.maxCX - fullResCoverage_.minCX) / 2;
    }

    constexpr size_t K_INDEX_BYTES = 4; // uint32_t
    size_t totalBytes = 0;
    for (const auto& [key, gpu] : gpuSections_) {
        if (gpu.resident)
            totalBytes += gpu.vertexCount * gpu.mesh.vertexStrideBytes + gpu.mesh.indexCount * K_INDEX_BYTES;
    }
    info.estimatedGpuBytes = totalBytes;
    return info;
}

} // namespace recurse::systems
