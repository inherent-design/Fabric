#include "recurse/systems/LODSystem.hh"

#include "fabric/core/AppContext.hh"
#include "fabric/core/Log.hh"
#include "fabric/core/Spatial.hh"
#include "fabric/core/SystemRegistry.hh"
#include "fabric/platform/ConfigManager.hh"
#include "fabric/platform/JobScheduler.hh"
#include "fabric/render/Camera.hh"
#include "fabric/render/SceneView.hh"
#include "fabric/world/ChunkedGrid.hh"
#include "recurse/render/LODGrid.hh"
#include "recurse/render/LODMeshManager.hh"
#include "recurse/simulation/MaterialRegistry.hh"
#include "recurse/simulation/SimulationGrid.hh"
#include "recurse/simulation/VoxelMaterial.hh"
#include "recurse/systems/TerrainSystem.hh"
#include "recurse/systems/VoxelRenderSystem.hh"
#include "recurse/systems/VoxelSimulationSystem.hh"
#include "recurse/world/SmoothVoxelVertex.hh"
#include "recurse/world/WorldGenerator.hh"

#include <algorithm>
#include <cmath>

namespace recurse::systems {

LODSystem::LODSystem() : grid_(std::make_unique<LODGrid>()) {}

LODSystem::~LODSystem() = default;

void LODSystem::doInit(fabric::AppContext& ctx) {
    uploadBudget_ = ctx.configManager.get<int>("lod.upload_budget", 50);
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

    auto* terrainSystem = ctx.systemRegistry.get<TerrainSystem>();
    if (terrainSystem)
        worldGen_ = &terrainSystem->worldGenerator();
}

void LODSystem::doShutdown() {
    gpuSections_.clear();
    visibleSections_.clear();
    pendingChunks_.clear();
    pendingDirectChunks_.clear();
    simGrid_ = nullptr;
    materials_ = nullptr;
    scheduler_ = nullptr;
    voxelRenderer_ = nullptr;
    worldGen_ = nullptr;

    FABRIC_LOG_INFO("LODSystem shut down");
}

void LODSystem::fixedUpdate(fabric::AppContext& /*ctx*/, float /*fixedDt*/) {
    if (!scheduler_ || !grid_)
        return;

    int dispatchedThisFrame = 0;
    constexpr int K_MAX_PER_FRAME = 10;

    // Process full-res LOD0 sections (from SimulationGrid data)
    while (simGrid_ && !pendingChunks_.empty() && dispatchedThisFrame < K_MAX_PER_FRAME) {
        auto [cx, cy, cz] = pendingChunks_.front();
        pendingChunks_.pop_front();

        auto* section = grid_->getOrCreate(0, cx, cy, cz);
        if (!section)
            continue;

        auto* grid = simGrid_;
        scheduler_->submitBackground([section, grid, cx, cy, cz]() {
            section->origin = Vec3i(cx * LODGrid::K_SECTION_WORLD_SIZE, cy * LODGrid::K_SECTION_WORLD_SIZE,
                                    cz * LODGrid::K_SECTION_WORLD_SIZE);
            section->palette.clear();
            section->palette.push_back(1);
            section->blockIndices.assign(LODSection::K_VOLUME, 0);

            for (int lz = 0; lz < LODSection::K_SIZE; ++lz) {
                for (int ly = 0; ly < LODSection::K_SIZE; ++ly) {
                    for (int lx = 0; lx < LODSection::K_SIZE; ++lx) {
                        int wx = section->origin.x + lx;
                        int wy = section->origin.y + ly;
                        int wz = section->origin.z + lz;
                        auto cell = grid->readCell(wx, wy, wz);

                        uint16_t matId = cell.materialId;
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
        });

        ++dispatchedThisFrame;
    }

    // Process direct LOD sections (from WorldGenerator point queries)
    while (worldGen_ && !pendingDirectChunks_.empty() && dispatchedThisFrame < K_MAX_PER_FRAME) {
        auto [cx, cy, cz] = pendingDirectChunks_.front();
        pendingDirectChunks_.pop_front();

        auto* section = grid_->getOrCreate(0, cx, cy, cz);
        if (!section)
            continue;

        auto* gen = worldGen_;
        scheduler_->submitBackground([section, gen, cx, cy, cz]() {
            section->origin = Vec3i(cx * LODGrid::K_SECTION_WORLD_SIZE, cy * LODGrid::K_SECTION_WORLD_SIZE,
                                    cz * LODGrid::K_SECTION_WORLD_SIZE);
            section->palette.clear();
            section->palette.push_back(1);
            section->blockIndices.assign(LODSection::K_VOLUME, 0);

            for (int lz = 0; lz < LODSection::K_SIZE; ++lz) {
                for (int ly = 0; ly < LODSection::K_SIZE; ++ly) {
                    for (int lx = 0; lx < LODSection::K_SIZE; ++lx) {
                        int wx = section->origin.x + lx;
                        int wy = section->origin.y + ly;
                        int wz = section->origin.z + lz;

                        uint16_t matId = gen->sampleMaterial(wx, wy, wz);
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
        });

        ++dispatchedThisFrame;
    }
}

void LODSystem::render(fabric::AppContext& ctx) {
    if (!grid_) {
        return;
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

        // Skip empty sections
        bool hasSolid = false;
        for (uint16_t idx : section.blockIndices) {
            if (section.materialOf(idx) != 0) {
                hasSolid = true;
                break;
            }
        }
        if (!hasSolid) {
            section.dirty = false;
            return;
        }

        auto key = LODSectionKey::make(section.level, section.origin.x / LODGrid::K_SECTION_WORLD_SIZE,
                                       section.origin.y / LODGrid::K_SECTION_WORLD_SIZE,
                                       section.origin.z / LODGrid::K_SECTION_WORLD_SIZE);

        // Skip if already uploaded and resident
        auto it = gpuSections_.find(key.value);
        if (it != gpuSections_.end() && it->second.resident) {
            section.dirty = false;
            return;
        }

        // Mesh section if meshManager_ is available
        if (meshManager_) {
            auto mesh = meshManager_->meshSection(section);
            if (!mesh.empty()) {
                uploadSection(key, mesh);
                ++uploaded;
                FABRIC_LOG_DEBUG("LODSystem: Uploaded section level={} ({},{},{}) verts={}", section.level, key.x(),
                                 key.y(), key.z(), mesh.vertices.size());
            }
        }

        section.dirty = false;

        // Try to build parent LOD section
        int sx = section.origin.x / LODGrid::K_SECTION_WORLD_SIZE;
        int sy = section.origin.y / LODGrid::K_SECTION_WORLD_SIZE;
        int sz = section.origin.z / LODGrid::K_SECTION_WORLD_SIZE;
        grid_->tryBuildParent(section.level, sx, sy, sz);
    });

    // Submit visible LOD sections
    if (voxelRenderer_ && grid_) {
        auto* camera = ctx.camera;
        if (camera) {
            selectVisibleSections(*camera, baseRadius_);

            if (!visibleSections_.empty()) {
                std::vector<recurse::ChunkMesh> lodMeshes;
                std::vector<recurse::ChunkRenderInfo> lodBatch;
                lodMeshes.reserve(visibleSections_.size());
                lodBatch.reserve(visibleSections_.size());

                for (const auto& vis : visibleSections_) {
                    auto it = gpuSections_.find(vis.key.value);
                    if (it == gpuSections_.end() || !it->second.resident)
                        continue;

                    const auto& gpu = it->second;

                    lodMeshes.push_back(recurse::ChunkMesh{
                        .vbh = gpu.vbh.get(),
                        .ibh = gpu.ibh.get(),
                        .indexCount = gpu.indexCount,
                        .palette = gpu.palette,
                        .valid = true,
                    });

                    auto worldOrigin = fabric::Vector3<double, fabric::Space::World>(
                        static_cast<double>(vis.section->origin.x), static_cast<double>(vis.section->origin.y),
                        static_cast<double>(vis.section->origin.z));
                    auto relOrigin = camera->cameraRelative(worldOrigin);

                    lodBatch.push_back(recurse::ChunkRenderInfo{
                        .mesh = nullptr,
                        .offsetX = relOrigin.x,
                        .offsetY = relOrigin.y,
                        .offsetZ = relOrigin.z,
                        .sortKey = vis.key.value,
                    });
                }

                for (size_t i = 0; i < lodBatch.size(); ++i)
                    lodBatch[i].mesh = &lodMeshes[i];

                if (!lodBatch.empty()) {
                    voxelRenderer_->renderBatch(ctx.sceneView->geometryViewId(), lodBatch.data(),
                                                static_cast<uint32_t>(lodBatch.size()));
                }
            }
        }
    }
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

void LODSystem::selectVisibleSections(const fabric::Camera& camera, float baseRadius) {
    visibleSections_.clear();

    auto camPosRaw = camera.getPosition();
    Vec3f camPos{camPosRaw.x, camPosRaw.y, camPosRaw.z};

    grid_->forEach([this, &camPos, baseRadius](const LODSection& section) {
        // Compute world-space center of section
        int scale = 1 << section.level;
        float worldSize = static_cast<float>(LODSection::K_SIZE * scale);
        Vec3f center{static_cast<float>(section.origin.x) + worldSize * 0.5f,
                     static_cast<float>(section.origin.y) + worldSize * 0.5f,
                     static_cast<float>(section.origin.z) + worldSize * 0.5f};

        // Distance from camera
        float dx = center.x - camPos.x;
        float dy = center.y - camPos.y;
        float dz = center.z - camPos.z;
        float distance = std::sqrt(dx * dx + dy * dy + dz * dz);

        // LOD selection: level = floor(log2(distance / (baseRadius * LODSection::K_SIZE)))
        int desiredLevel = 1;
        float threshold = baseRadius * LODSection::K_SIZE;
        while (distance > threshold && desiredLevel < maxLODLevel_) {
            threshold *= 2.0f;
            ++desiredLevel;
        }

        // Only include sections at the desired LOD level
        if (section.level != desiredLevel) {
            return;
        }

        // Check if section has GPU mesh
        auto key = LODSectionKey::make(section.level, section.origin.x / LODGrid::K_SECTION_WORLD_SIZE,
                                       section.origin.y / LODGrid::K_SECTION_WORLD_SIZE,
                                       section.origin.z / LODGrid::K_SECTION_WORLD_SIZE);
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
    bgfx::VertexLayout layout = SmoothVoxelVertex::getVertexLayout();
    gpu.vbh.reset(bgfx::createVertexBuffer(
        bgfx::copy(mesh.vertices.data(), static_cast<uint32_t>(mesh.vertices.size() * sizeof(SmoothVoxelVertex))),
        layout));

    // Create index buffer (copy: mesh data is temporary); reset destroys old handle
    gpu.ibh.reset(bgfx::createIndexBuffer(
        bgfx::copy(mesh.indices.data(), static_cast<uint32_t>(mesh.indices.size() * sizeof(uint32_t))),
        BGFX_BUFFER_INDEX32));

    gpu.vertexCount = static_cast<uint32_t>(mesh.vertices.size());
    gpu.indexCount = static_cast<uint32_t>(mesh.indices.size());
    gpu.palette = mesh.palette;
    gpu.resident = true;
}

void LODSystem::releaseGPUSection(LODSectionKey key) {
    gpuSections_.erase(key.value);
}

LODDebugInfo LODSystem::debugInfo() const {
    LODDebugInfo info;
    info.pendingSections = static_cast<int>(pendingChunks_.size());
    info.gpuResidentSections = static_cast<int>(gpuSections_.size());
    info.visibleSections = static_cast<int>(visibleSections_.size());

    constexpr size_t K_VERTEX_BYTES = 32; // SmoothVoxelVertex
    constexpr size_t K_INDEX_BYTES = 4;   // uint32_t
    size_t totalBytes = 0;
    for (const auto& [key, gpu] : gpuSections_) {
        if (gpu.resident)
            totalBytes += gpu.vertexCount * K_VERTEX_BYTES + gpu.indexCount * K_INDEX_BYTES;
    }
    info.estimatedGpuBytes = totalBytes;
    return info;
}

} // namespace recurse::systems
