#include "recurse/systems/WaterRenderSystem.hh"

#include "fabric/core/AppContext.hh"
#include "fabric/core/Log.hh"
#include "fabric/core/SceneView.hh"
#include "fabric/core/SystemRegistry.hh"
#include "fabric/core/Temporal.hh"
#include "fabric/utils/Profiler.hh"
#include "recurse/render/WaterSimulation.hh"
#include "recurse/systems/ChunkPipelineSystem.hh"
#include "recurse/systems/OITRenderSystem.hh"
#include "recurse/systems/ShadowRenderSystem.hh"
#include "recurse/systems/TerrainSystem.hh"
#include "recurse/systems/VoxelRenderSystem.hh"
#include "recurse/world/ChunkedGrid.hh"
#include "recurse/world/TerrainGenerator.hh"
#include "recurse/world/VoxelMesher.hh"

#include <unordered_set>

namespace recurse::systems {

WaterRenderSystem::WaterRenderSystem() = default;
WaterRenderSystem::~WaterRenderSystem() = default;

void WaterRenderSystem::init(fabric::AppContext& ctx) {
    terrain_ = ctx.systemRegistry.get<TerrainSystem>();
    chunks_ = ctx.systemRegistry.get<ChunkPipelineSystem>();
    shadow_ = ctx.systemRegistry.get<ShadowRenderSystem>();

    waterSim_ = std::make_unique<WaterSimulation>();

    // Register change callback: when water simulation changes a cell,
    // mark the containing chunk as needing a mesh rebuild.
    waterSim_->setWaterChangeCallback([this](const WaterChangeEvent& e) {
        int cx = (e.x >= 0) ? e.x / kChunkSize : (e.x - kChunkSize + 1) / kChunkSize;
        int cy = (e.y >= 0) ? e.y / kChunkSize : (e.y - kChunkSize + 1) / kChunkSize;
        int cz = (e.z >= 0) ? e.z / kChunkSize : (e.z - kChunkSize + 1) / kChunkSize;
        dirtyWaterChunks_.insert({cx, cy, cz});
    });

    // Seed water for all currently loaded chunks
    for (const auto& [coord, _] : chunks_->chunkEntities()) {
        seedWaterForChunk(coord.cx, coord.cy, coord.cz);
        dirtyWaterChunks_.insert(coord);
    }

    // Build initial water meshes (budgeted across multiple passes)
    {
        FABRIC_ZONE_SCOPED_N("initial_water_mesh");
        int built = 0;
        auto it = dirtyWaterChunks_.begin();
        while (it != dirtyWaterChunks_.end()) {
            rebuildWaterMesh(it->cx, it->cy, it->cz);
            it = dirtyWaterChunks_.erase(it);
            ++built;
        }
        FABRIC_LOG_INFO("WaterRenderSystem initialized: {} initial water meshes, {} total water mesh entries", built,
                        waterMeshes_.size());
    }
}

void WaterRenderSystem::render(fabric::AppContext& ctx) {
    FABRIC_ZONE_SCOPED_N("water_render");

    auto* sceneView = ctx.sceneView;
    float elapsed = static_cast<float>(ctx.timeline.getCurrentTime());

    // Advance water simulation using the density grid and a fixed dt
    // (simulation is frame-rate independent via internal budgeting)
    {
        FABRIC_ZONE_SCOPED_N("water_sim_step");
        waterSim_->step(terrain_->densityGrid(), 1.0f / 60.0f);
    }

    // Seed water for newly loaded chunks (chunks present in pipeline but
    // not yet in our water mesh cache and not already marked dirty)
    for (const auto& [coord, _] : chunks_->chunkEntities()) {
        if (waterMeshes_.find(coord) == waterMeshes_.end() &&
            dirtyWaterChunks_.find(coord) == dirtyWaterChunks_.end()) {
            seedWaterForChunk(coord.cx, coord.cy, coord.cz);
            dirtyWaterChunks_.insert(coord);
        }
    }

    // Unload water meshes for chunks that are no longer loaded
    {
        auto it = waterMeshes_.begin();
        while (it != waterMeshes_.end()) {
            if (chunks_->chunkEntities().find(it->first) == chunks_->chunkEntities().end()) {
                VoxelMesher::destroyWaterMesh(it->second);
                it = waterMeshes_.erase(it);
            } else {
                ++it;
            }
        }
    }

    // Rebuild dirty water meshes (budgeted)
    {
        FABRIC_ZONE_SCOPED_N("water_mesh_rebuild");
        int rebuilt = 0;
        auto it = dirtyWaterChunks_.begin();
        while (it != dirtyWaterChunks_.end() && rebuilt < kMaxWaterMeshRebuildsPerFrame) {
            rebuildWaterMesh(it->cx, it->cy, it->cz);
            it = dirtyWaterChunks_.erase(it);
            ++rebuilt;
        }
    }

    // Set renderer uniforms
    auto& lightDir = shadow_->lightDirection();
    waterRenderer_.setLightDirection(fabric::Vector3<float, fabric::Space::World>(lightDir.x, lightDir.y, lightDir.z));
    waterRenderer_.setTime(elapsed);

    // Frustum cull: only render water meshes for visible chunk entities
    std::unordered_set<flecs::entity_t> visibleEntityIds;
    visibleEntityIds.reserve(sceneView->visibleEntities().size());
    for (const auto& entity : sceneView->visibleEntities()) {
        visibleEntityIds.insert(entity.id());
    }

    // Submit water meshes to the transparent view
    bgfx::ViewId waterView = sceneView->transparentViewId();
    for (const auto& [coord, mesh] : waterMeshes_) {
        if (!mesh.valid || mesh.indexCount == 0)
            continue;

        auto entIt = chunks_->chunkEntities().find(coord);
        if (entIt == chunks_->chunkEntities().end())
            continue;
        if (visibleEntityIds.find(entIt->second.id()) == visibleEntityIds.end())
            continue;

        waterRenderer_.render(waterView, mesh, coord.cx, coord.cy, coord.cz);
    }
}

void WaterRenderSystem::shutdown() {
    for (auto& [_, mesh] : waterMeshes_) {
        VoxelMesher::destroyWaterMesh(mesh);
    }
    waterMeshes_.clear();
    dirtyWaterChunks_.clear();

    waterRenderer_.shutdown();
    waterSim_.reset();

    FABRIC_LOG_INFO("WaterRenderSystem shut down");
}

void WaterRenderSystem::configureDependencies() {
    after<VoxelRenderSystem>();
    after<ChunkPipelineSystem>();
    after<ShadowRenderSystem>();
    before<OITRenderSystem>();
}

void WaterRenderSystem::seedWaterForChunk(int cx, int cy, int cz) {
    float x0 = static_cast<float>(cx * kChunkSize);
    float y0 = static_cast<float>(cy * kChunkSize);
    float z0 = static_cast<float>(cz * kChunkSize);
    float x1 = x0 + static_cast<float>(kChunkSize);
    float y1 = y0 + static_cast<float>(kChunkSize);
    float z1 = z0 + static_cast<float>(kChunkSize);
    fabric::AABB region(fabric::Vec3f(x0, y0, z0), fabric::Vec3f(x1, y1, z1));

    terrain_->terrainGen().generateWater(terrain_->density(), waterField_, region);

    // Also copy into the simulation's water field so the cellular automaton
    // has initial data to work with.
    auto& simField = waterSim_->waterField();
    int minX = cx * kChunkSize;
    int minY = cy * kChunkSize;
    int minZ = cz * kChunkSize;
    for (int z = minZ; z < minZ + kChunkSize; ++z) {
        for (int y = minY; y < minY + kChunkSize; ++y) {
            for (int x = minX; x < minX + kChunkSize; ++x) {
                float w = waterField_.read(x, y, z);
                if (w > 0.0f) {
                    simField.write(x, y, z, w);
                }
            }
        }
    }
}

bool WaterRenderSystem::rebuildWaterMesh(int cx, int cy, int cz) {
    ChunkCoord coord{cx, cy, cz};

    // Destroy old mesh if present
    if (auto it = waterMeshes_.find(coord); it != waterMeshes_.end()) {
        VoxelMesher::destroyWaterMesh(it->second);
        waterMeshes_.erase(it);
    }

    // Use the simulation's water field (reflects dynamic changes)
    const auto& simWater = waterSim_->waterField();
    auto mesh = VoxelMesher::meshWaterChunk(cx, cy, cz, simWater, terrain_->densityGrid());

    if (mesh.valid && mesh.indexCount > 0) {
        waterMeshes_[coord] = mesh;
        return true;
    }

    // Mesh was empty (no water geometry in this chunk), destroy handles
    if (mesh.valid) {
        VoxelMesher::destroyWaterMesh(mesh);
    }
    return false;
}

} // namespace recurse::systems
