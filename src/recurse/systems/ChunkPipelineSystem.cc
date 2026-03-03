#include "recurse/systems/ChunkPipelineSystem.hh"
#include "recurse/systems/CharacterMovementSystem.hh"
#include "recurse/systems/PhysicsGameSystem.hh"
#include "recurse/systems/TerrainSystem.hh"

#include "fabric/core/AppContext.hh"
#include "fabric/core/ECS.hh"
#include "fabric/core/Event.hh"
#include "fabric/core/Log.hh"
#include "fabric/core/SystemRegistry.hh"
#include "fabric/utils/Profiler.hh"
#include "recurse/world/ChunkMeshManager.hh"

#include <bgfx/bgfx.h>
#include <cmath>
#include <limits>

namespace {
constexpr float kSpawnX = 16.0f;
constexpr float kSpawnY = 48.0f;
constexpr float kSpawnZ = 16.0f;
} // namespace

namespace recurse::systems {

ChunkPipelineSystem::~ChunkPipelineSystem() = default;

void ChunkPipelineSystem::init(fabric::AppContext& ctx) {
    terrain_ = ctx.systemRegistry.get<TerrainSystem>();
    physics_ = ctx.systemRegistry.get<PhysicsGameSystem>();
    charMovement_ = ctx.systemRegistry.get<CharacterMovementSystem>();

    auto& dispatcher = ctx.dispatcher;

    // Chunk mesh management (CPU side, budgeted re-meshing)
    meshManager_ =
        std::make_unique<ChunkMeshManager>(dispatcher, terrain_->density().grid(), terrain_->essence().grid());

    // Chunk streaming
    StreamingConfig streamConfig;
    streamConfig.baseRadius = 3;
    streamConfig.maxRadius = 5;
    streamConfig.maxLoadsPerTick = 2;
    streamConfig.maxUnloadsPerTick = 4;
    streaming_ = std::make_unique<ChunkStreamingManager>(streamConfig);

    // When voxel data changes, queue the chunk for GPU re-upload
    dispatcher.addEventListener(kVoxelChangedEvent, [this](fabric::Event& e) {
        int cx = e.getData<int>("cx");
        int cy = e.getData<int>("cy");
        int cz = e.getData<int>("cz");
        gpuUploadQueue_.insert({cx, cy, cz});
    });

    // Initial terrain generation + meshing
    {
        FABRIC_ZONE_SCOPED_N("initial_terrain");
        auto& ecsWorld = ctx.world;
        auto initLoad = streaming_->update(kSpawnX, kSpawnY, kSpawnZ, 0.0f);

        for (const auto& coord : initLoad.toLoad) {
            terrain_->generateChunkTerrain(coord.cx, coord.cy, coord.cz);
            meshManager_->markDirty(coord.cx, coord.cy, coord.cz);

            auto ent = ecsWorld.get().entity().add<fabric::SceneEntity>().set<fabric::BoundingBox>(
                {static_cast<float>(coord.cx * kChunkSize), static_cast<float>(coord.cy * kChunkSize),
                 static_cast<float>(coord.cz * kChunkSize), static_cast<float>((coord.cx + 1) * kChunkSize),
                 static_cast<float>((coord.cy + 1) * kChunkSize), static_cast<float>((coord.cz + 1) * kChunkSize)});
            chunkEntities_[coord] = ent;
        }

        // Flush dirty chunks for initial load with bounded passes
        constexpr int kMaxInitialRemeshPasses = 512;
        constexpr int kMaxInitialNoProgressPasses = 8;

        size_t previousDirty = std::numeric_limits<size_t>::max();
        int noProgressPasses = 0;
        int totalRemeshed = 0;

        for (int pass = 0; pass < kMaxInitialRemeshPasses; ++pass) {
            size_t dirtyBefore = meshManager_->dirtyCount();
            if (dirtyBefore == 0)
                break;

            int remeshed = meshManager_->update();
            totalRemeshed += remeshed;

            size_t dirtyAfter = meshManager_->dirtyCount();
            if (dirtyAfter >= dirtyBefore || dirtyAfter >= previousDirty) {
                ++noProgressPasses;
            } else {
                noProgressPasses = 0;
            }
            previousDirty = dirtyAfter;

            if (noProgressPasses >= kMaxInitialNoProgressPasses) {
                FABRIC_LOG_WARN("Initial terrain remesh made no progress for {} passes; deferring {} chunks to runtime",
                                noProgressPasses, dirtyAfter);
                break;
            }
        }

        // Upload all ready initial meshes to GPU
        for (const auto& coord : initLoad.toLoad) {
            if (meshManager_->isDirty(coord))
                continue;

            const auto* data = meshManager_->meshFor(coord);
            if (data && !data->vertices.empty()) {
                gpuMeshes_[coord] = uploadChunkMesh(*data);
            }
        }

        FABRIC_LOG_INFO(
            "Initial terrain: {} chunks loaded, {} remeshed, {} GPU meshes, {} chunks pending runtime remesh",
            initLoad.toLoad.size(), totalRemeshed, gpuMeshes_.size(), meshManager_->dirtyCount());
    }
}

void ChunkPipelineSystem::shutdown() {
    for (auto& [_, mesh] : gpuMeshes_) {
        VoxelMesher::destroyMesh(mesh);
    }
    gpuMeshes_.clear();

    for (auto& [_, entity] : chunkEntities_) {
        entity.destruct();
    }
    chunkEntities_.clear();
}

void ChunkPipelineSystem::fixedUpdate(fabric::AppContext& ctx, float /*fixedDt*/) {
    auto& ecsWorld = ctx.world;

    // Streaming: load/unload chunks around player position
    float px = kSpawnX, py = kSpawnY, pz = kSpawnZ;
    float speed = 0.0f;

    if (charMovement_) {
        const auto& pos = charMovement_->playerPosition();
        px = pos.x;
        py = pos.y;
        pz = pos.z;
        const auto& vel = charMovement_->playerVelocity();
        speed = std::sqrt(vel.x * vel.x + vel.y * vel.y + vel.z * vel.z);
    }

    auto streamUpdate = streaming_->update(px, py, pz, speed);

    for (const auto& coord : streamUpdate.toLoad) {
        terrain_->generateChunkTerrain(coord.cx, coord.cy, coord.cz);
        meshManager_->markDirty(coord.cx, coord.cy, coord.cz);
        gpuUploadQueue_.insert(coord);

        // Re-mesh 6 neighbors so cross-boundary faces are re-culled
        // against the newly loaded chunk data (fixes BUG-CHUNKFACE).
        static constexpr int kNeighborOffsets[6][3] = {
            {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1},
        };
        for (const auto& off : kNeighborOffsets) {
            ChunkCoord neighbor{coord.cx + off[0], coord.cy + off[1], coord.cz + off[2]};
            if (chunkEntities_.find(neighbor) != chunkEntities_.end()) {
                meshManager_->markDirty(neighbor.cx, neighbor.cy, neighbor.cz);
                gpuUploadQueue_.insert(neighbor);
            }
        }

        if (chunkEntities_.find(coord) == chunkEntities_.end()) {
            auto ent = ecsWorld.get().entity().add<fabric::SceneEntity>().set<fabric::BoundingBox>(
                {static_cast<float>(coord.cx * kChunkSize), static_cast<float>(coord.cy * kChunkSize),
                 static_cast<float>(coord.cz * kChunkSize), static_cast<float>((coord.cx + 1) * kChunkSize),
                 static_cast<float>((coord.cy + 1) * kChunkSize), static_cast<float>((coord.cz + 1) * kChunkSize)});
            chunkEntities_[coord] = ent;
        }
    }
    for (const auto& coord : streamUpdate.toUnload) {
        gpuUploadQueue_.erase(coord);
        meshManager_->removeChunk(coord);
        if (physics_)
            physics_->physicsWorld().removeChunkCollision(coord.cx, coord.cy, coord.cz);

        if (auto it = chunkEntities_.find(coord); it != chunkEntities_.end()) {
            it->second.destruct();
            chunkEntities_.erase(it);
        }
        if (auto it = gpuMeshes_.find(coord); it != gpuMeshes_.end()) {
            VoxelMesher::destroyMesh(it->second);
            gpuMeshes_.erase(it);
        }
        terrain_->density().grid().removeChunk(coord.cx, coord.cy, coord.cz);
        terrain_->essence().grid().removeChunk(coord.cx, coord.cy, coord.cz);
    }

    // LOD: compute per-chunk LOD from player distance (marks dirty on change)
    meshManager_->updateLOD(px, py, pz);

    // Mesh manager: budgeted CPU re-meshing of dirty chunks
    meshManager_->update();

    // GPU mesh sync: upload re-meshed chunks
    {
        FABRIC_ZONE_SCOPED_N("chunk_mesh_upload");
        auto it = gpuUploadQueue_.begin();
        while (it != gpuUploadQueue_.end()) {
            if (chunkEntities_.find(*it) == chunkEntities_.end()) {
                it = gpuUploadQueue_.erase(it);
                continue;
            }

            if (!meshManager_->isDirty(*it)) {
                const auto* data = meshManager_->meshFor(*it);
                if (auto git = gpuMeshes_.find(*it); git != gpuMeshes_.end()) {
                    VoxelMesher::destroyMesh(git->second);
                    gpuMeshes_.erase(git);
                }
                if (data && !data->vertices.empty()) {
                    gpuMeshes_[*it] = uploadChunkMesh(*data);
                }
                it = gpuUploadQueue_.erase(it);
            } else {
                ++it;
            }
        }
    }
}

void ChunkPipelineSystem::configureDependencies() {
    after<TerrainSystem>();
    after<PhysicsGameSystem>();
    after<CharacterMovementSystem>();
}

size_t ChunkPipelineSystem::dirtyCount() const {
    return meshManager_ ? meshManager_->dirtyCount() : 0;
}

ChunkMesh ChunkPipelineSystem::uploadChunkMesh(const ChunkMeshData& data) {
    ChunkMesh mesh;
    if (data.vertices.empty())
        return mesh;

    auto layout = VoxelMesher::getVertexLayout();
    mesh.vbh = bgfx::createVertexBuffer(
        bgfx::copy(data.vertices.data(), static_cast<uint32_t>(data.vertices.size() * sizeof(VoxelVertex))), layout);
    mesh.ibh = bgfx::createIndexBuffer(
        bgfx::copy(data.indices.data(), static_cast<uint32_t>(data.indices.size() * sizeof(uint32_t))),
        BGFX_BUFFER_INDEX32);
    mesh.indexCount = static_cast<uint32_t>(data.indices.size());
    mesh.palette = data.palette;
    mesh.valid = true;
    return mesh;
}

} // namespace recurse::systems
