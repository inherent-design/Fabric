#include "recurse/systems/LODSystem.hh"

#include "fabric/core/AppContext.hh"
#include "fabric/core/Camera.hh"
#include "fabric/core/Log.hh"
#include "fabric/simulation/MaterialRegistry.hh"
#include "fabric/simulation/SimulationGrid.hh"
#include "fabric/simulation/VoxelMaterial.hh"
#include "recurse/render/LODGrid.hh"
#include "recurse/render/LODMeshManager.hh"
#include "recurse/systems/ChunkPipelineSystem.hh"
#include "recurse/systems/VoxelMeshingSystem.hh"
#include "recurse/world/ChunkedGrid.hh"
#include "recurse/world/SmoothVoxelVertex.hh"

#include <algorithm>
#include <cmath>

namespace recurse::systems {

LODSystem::LODSystem() : grid_(std::make_unique<LODGrid>()) {}

LODSystem::~LODSystem() {
    shutdown();
}

void LODSystem::init(fabric::AppContext& /*ctx*/) {
    // Note: In a full implementation, materials would be obtained from ctx
    // For now, we LODSystem doesn't require materials for basic operation
    FABRIC_LOG_INFO("[LODSystem] Initialized");
}

void LODSystem::shutdown() {
    // Destroy GPU resources
    for (auto& [key, gpu] : gpuSections_) {
        releaseGPUSection(LODSectionKey{key});
    }
    gpuSections_.clear();
    visibleSections_.clear();
    pendingChunks_.clear();

    FABRIC_LOG_INFO("[LODSystem] Shutdown complete");
}

void LODSystem::fixedUpdate(fabric::AppContext& /*ctx*/, float /*fixedDt*/) {
    // Process pending chunks: build LOD0 sections
    int processedThisFrame = 0;
    constexpr int kMaxPerFrame = 10;

    while (!pendingChunks_.empty() && processedThisFrame < kMaxPerFrame) {
        auto [cx, cy, cz] = pendingChunks_.front();
        pendingChunks_.pop_front();
        buildLOD0Section(cx, cy, cz);
        ++processedThisFrame;
    }
}

void LODSystem::render(fabric::AppContext& /*ctx*/) {
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

        auto key = LODSectionKey::make(section.level, section.origin.x / LODGrid::kSectionWorldSize,
                                       section.origin.y / LODGrid::kSectionWorldSize,
                                       section.origin.z / LODGrid::kSectionWorldSize);

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
        int sx = section.origin.x / LODGrid::kSectionWorldSize;
        int sy = section.origin.y / LODGrid::kSectionWorldSize;
        int sz = section.origin.z / LODGrid::kSectionWorldSize;
        grid_->tryBuildParent(section.level, sx, sy, sz);
    });
}

void LODSystem::configureDependencies() {
    // Dependencies resolved in init() or via setters
}

void LODSystem::setMaterialRegistry(const fabric::simulation::MaterialRegistry* materials) {
    materials_ = materials;
    if (materials_ && grid_) {
        meshManager_ = std::make_unique<LODMeshManager>(*grid_, *materials_);
        FABRIC_LOG_INFO("[LODSystem] LODMeshManager created with MaterialRegistry");
    }
}

void LODSystem::buildLOD0Section(int cx, int cy, int cz) {
    if (!simGrid_) {
        return;
    }

    // Create LOD0 section
    int sx = cx;
    int sy = cy;
    int sz = cz;
    auto* section = grid_->getOrCreate(0, sx, sy, sz);
    section->origin =
        glm::ivec3(cx * LODGrid::kSectionWorldSize, cy * LODGrid::kSectionWorldSize, cz * LODGrid::kSectionWorldSize);
    section->palette.clear();
    section->palette.push_back(1); // Index 0 = air (materialId 1)
    section->blockIndices.assign(LODSection::kVolume, 0);
    section->dirty = true;

    // Fill section data from SimulationGrid
    for (int lz = 0; lz < LODSection::kSize; ++lz) {
        for (int ly = 0; ly < LODSection::kSize; ++ly) {
            for (int lx = 0; lx < LODSection::kSize; ++lx) {
                int wx = section->origin.x + lx;
                int wy = section->origin.y + ly;
                int wz = section->origin.z + lz;
                auto cell = simGrid_->readCell(wx, wy, wz);

                // Map materialId to palette
                uint16_t matId = cell.materialId;
                uint16_t palIdx = 0;

                // Find existing palette entry or add new one
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

    // Try to build parent LOD section
    grid_->tryBuildParent(0, sx, sy, sz);
}

void LODSystem::onChunkReady(int cx, int cy, int cz) {
    pendingChunks_.emplace_back(cx, cy, cz);
}

void LODSystem::selectVisibleSections(const fabric::Camera& camera, float baseRadius) {
    visibleSections_.clear();

    auto camPosRaw = camera.getPosition();
    glm::vec3 camPos{camPosRaw.x, camPosRaw.y, camPosRaw.z};

    grid_->forEach([this, &camPos, baseRadius](const LODSection& section) {
        // Compute world-space center of section
        int scale = 1 << section.level;
        float worldSize = static_cast<float>(LODSection::kSize * scale);
        glm::vec3 center{static_cast<float>(section.origin.x) + worldSize * 0.5f,
                         static_cast<float>(section.origin.y) + worldSize * 0.5f,
                         static_cast<float>(section.origin.z) + worldSize * 0.5f};

        // Distance from camera
        float dx = center.x - camPos.x;
        float dy = center.y - camPos.y;
        float dz = center.z - camPos.z;
        float distance = std::sqrt(dx * dx + dy * dy + dz * dz);

        // LOD selection: level = floor(log2(distance / (baseRadius * LODSection::kSize)))
        int desiredLevel = 1;
        float threshold = baseRadius * LODSection::kSize;
        while (distance > threshold && desiredLevel < maxLODLevel_) {
            threshold *= 2.0f;
            ++desiredLevel;
        }

        // Only include sections at the desired LOD level
        if (section.level != desiredLevel) {
            return;
        }

        // Check if section has GPU mesh
        auto key = LODSectionKey::make(section.level, section.origin.x / LODGrid::kSectionWorldSize,
                                       section.origin.y / LODGrid::kSectionWorldSize,
                                       section.origin.z / LODGrid::kSectionWorldSize);
        auto it = gpuSections_.find(key.value);
        if (it == gpuSections_.end() || !it->second.resident) {
            return;
        }

        VisibleSection vis;
        vis.section = const_cast<LODSection*>(&section);
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

    // Release old buffers
    if (bgfx::isValid(gpu.vbh)) {
        bgfx::destroy(gpu.vbh);
    }
    if (bgfx::isValid(gpu.ibh)) {
        bgfx::destroy(gpu.ibh);
    }

    // Create vertex buffer
    bgfx::VertexLayout layout = SmoothVoxelVertex::getVertexLayout();
    gpu.vbh = bgfx::createVertexBuffer(
        bgfx::makeRef(mesh.vertices.data(), mesh.vertices.size() * sizeof(SmoothVoxelVertex)), layout);

    // Create index buffer
    gpu.ibh = bgfx::createIndexBuffer(bgfx::makeRef(mesh.indices.data(), mesh.indices.size() * sizeof(uint32_t)));

    gpu.indexCount = static_cast<uint32_t>(mesh.indices.size());
    gpu.palette = mesh.palette;
    gpu.resident = true;
}

void LODSystem::releaseGPUSection(LODSectionKey key) {
    auto it = gpuSections_.find(key.value);
    if (it != gpuSections_.end()) {
        auto& gpu = it->second;
        if (bgfx::isValid(gpu.vbh)) {
            bgfx::destroy(gpu.vbh);
        }
        if (bgfx::isValid(gpu.ibh)) {
            bgfx::destroy(gpu.ibh);
        }
        gpuSections_.erase(it);
    }
}

} // namespace recurse::systems
