#include "recurse/systems/ChunkPipelineSystem.hh"
#include "recurse/character/GameConstants.hh"
#include "recurse/persistence/ChunkSaveService.hh"
#include "recurse/persistence/ChunkStore.hh"
#include "recurse/persistence/FilesystemChunkStore.hh"
#include "recurse/simulation/ChunkActivityTracker.hh"
#include "recurse/simulation/SimulationGrid.hh"
#include "recurse/systems/CharacterMovementSystem.hh"
#include "recurse/systems/LODSystem.hh"
#include "recurse/systems/PhysicsGameSystem.hh"
#include "recurse/systems/TerrainSystem.hh"
#include "recurse/systems/VoxelMeshingSystem.hh"
#include "recurse/systems/VoxelSimulationSystem.hh"
#include "recurse/world/WorldGenerator.hh"

#include "fabric/core/AppContext.hh"
#include "fabric/core/ECS.hh"
#include "fabric/core/Event.hh"
#include "fabric/core/Log.hh"
#include "fabric/core/SystemRegistry.hh"
#include "fabric/platform/ConfigManager.hh"
#include "fabric/utils/Profiler.hh"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <tuple>
#include <vector>

namespace recurse::systems {

ChunkPipelineSystem::~ChunkPipelineSystem() = default;

void ChunkPipelineSystem::doInit(fabric::AppContext& ctx) {
    lodSystem_ = ctx.systemRegistry.get<LODSystem>();
    terrain_ = ctx.systemRegistry.get<TerrainSystem>();
    meshingSystem_ = ctx.systemRegistry.get<VoxelMeshingSystem>();
    simSystem_ = ctx.systemRegistry.get<VoxelSimulationSystem>();
    physics_ = ctx.systemRegistry.get<PhysicsGameSystem>();
    charMovement_ = ctx.systemRegistry.get<CharacterMovementSystem>();

    // Chunk streaming — stress test: push render distance
    StreamingConfig streamConfig;
    streamConfig.baseRadius = ctx.configManager.get<int>("terrain.chunk_radius", 8);
    streamConfig.maxLoadsPerTick = 4;
    streamConfig.maxUnloadsPerTick = 8;
    streamConfig.maxTrackedChunks = 4096;
    streaming_ = std::make_unique<ChunkStreamingManager>(streamConfig);

    lodRadius_ = ctx.configManager.get<int>("lod.radius", 24);
    lodGenBudget_ = ctx.configManager.get<int>("lod.gen_budget", 4);
}

void ChunkPipelineSystem::doShutdown() {
    // Flush any pending chunk saves before destroying data
    if (saveService_)
        saveService_->flush();

    for (auto& [_, entity] : chunkEntities_) {
        entity.destruct();
    }
    chunkEntities_.clear();
}

void ChunkPipelineSystem::fixedUpdate(fabric::AppContext& ctx, float /*fixedDt*/) {
    FABRIC_ZONE_SCOPED_N("chunk_pipeline");
    auto& ecsWorld = ctx.world;

    // Use cached position for streaming (one-frame delay; invisible at chunk scale)
    auto streamUpdate = streaming_->update(lastPlayerX_, lastPlayerY_, lastPlayerZ_);

    loadsThisFrame_ = static_cast<int>(streamUpdate.toLoad.size());
    unloadsThisFrame_ = static_cast<int>(streamUpdate.toUnload.size());

    // Separate new chunks into disk-loaded and needs-generation
    std::vector<ChunkCoord> newChunks;
    std::vector<std::tuple<int, int, int>> toGenerate;
    for (const auto& coord : streamUpdate.toLoad) {
        if (chunkEntities_.find(coord) != chunkEntities_.end())
            continue;
        newChunks.push_back(coord);

        // Chunk may already exist in sim grid (e.g. from generateInitialWorld).
        // Skip disk load and generation; just create the ECS entity below.
        if (simSystem_ && simSystem_->simulationGrid().hasChunk(coord.cx, coord.cy, coord.cz))
            continue;

        bool loaded = tryLoadChunkFromDisk(coord.cx, coord.cy, coord.cz);
        if (!loaded)
            toGenerate.emplace_back(coord.cx, coord.cy, coord.cz);
    }

    // Parallel batch generation for chunks not loaded from disk
    if (!toGenerate.empty() && simSystem_)
        simSystem_->generateChunksBatch(toGenerate);

    // Sequential finalization: ECS entities + LOD notification
    for (const auto& coord : newChunks) {
        if (lodSystem_)
            lodSystem_->onChunkReady(coord.cx, coord.cy, coord.cz);

        auto ent = ecsWorld.get().entity().add<fabric::SceneEntity>().set<fabric::BoundingBox>(
            {static_cast<float>(coord.cx * K_CHUNK_SIZE), static_cast<float>(coord.cy * K_CHUNK_SIZE),
             static_cast<float>(coord.cz * K_CHUNK_SIZE), static_cast<float>((coord.cx + 1) * K_CHUNK_SIZE),
             static_cast<float>((coord.cy + 1) * K_CHUNK_SIZE), static_cast<float>((coord.cz + 1) * K_CHUNK_SIZE)});
        chunkEntities_[coord] = ent;
    }
    for (const auto& coord : streamUpdate.toUnload) {
        // Save chunk to disk before removing data
        saveChunkToDisk(coord.cx, coord.cy, coord.cz);

        // Remove GPU mesh (vertex/index buffers) before simulation data
        if (meshingSystem_)
            meshingSystem_->removeChunkMesh(fabric::ChunkCoord{coord.cx, coord.cy, coord.cz});

        // Remove LOD section and GPU resources
        if (lodSystem_)
            lodSystem_->onChunkRemoved(coord.cx, coord.cy, coord.cz);

        // Remove from simulation grid
        if (simSystem_)
            simSystem_->removeChunk(coord.cx, coord.cy, coord.cz);

        if (physics_) {
            physics_->removeDirtyChunk(coord.cx, coord.cy, coord.cz);
            physics_->physicsWorld().removeChunkCollision(coord.cx, coord.cy, coord.cz);
        }

        if (auto it = chunkEntities_.find(coord); it != chunkEntities_.end()) {
            it->second.destruct();
            chunkEntities_.erase(it);
        }
    }

    // Update cached position for next frame
    if (charMovement_) {
        const auto& pos = charMovement_->playerPosition();
        lastPlayerX_ = pos.x;
        lastPlayerY_ = pos.y;
        lastPlayerZ_ = pos.z;
    }

    // LOD ring: track chunks outside full-res radius, inside lod_radius
    int chunkRadius = streaming_ ? streaming_->currentRadius() : 0;
    if (lodRadius_ > chunkRadius) {
        int cx = static_cast<int>(std::floor(lastPlayerX_ / static_cast<float>(K_CHUNK_SIZE)));
        int cy = static_cast<int>(std::floor(lastPlayerY_ / static_cast<float>(K_CHUNK_SIZE)));
        int cz = static_cast<int>(std::floor(lastPlayerZ_ / static_cast<float>(K_CHUNK_SIZE)));
        updateLODRing(cx, cy, cz);
    }
}

void ChunkPipelineSystem::updateLODRing(int centerCX, int centerCY, int centerCZ) {
    int chunkRadius = streaming_ ? streaming_->currentRadius() : 0;

    std::unordered_set<ChunkCoord, ChunkCoordHash> desired;
    for (int dz = -lodRadius_; dz <= lodRadius_; ++dz) {
        for (int dy = -lodRadius_; dy <= lodRadius_; ++dy) {
            for (int dx = -lodRadius_; dx <= lodRadius_; ++dx) {
                if (std::abs(dx) <= chunkRadius && std::abs(dy) <= chunkRadius && std::abs(dz) <= chunkRadius)
                    continue;

                ChunkCoord c{centerCX + dx, centerCY + dy, centerCZ + dz};

                if (terrain_) {
                    int surfaceMax = terrain_->worldGenerator().maxSurfaceHeight(c.cx, c.cz);
                    int chunkBottomY = c.cy * K_CHUNK_SIZE;
                    int chunkTopY = (c.cy + 1) * K_CHUNK_SIZE;
                    if (chunkBottomY > surfaceMax || chunkTopY < surfaceMax - K_CHUNK_SIZE)
                        continue;
                }

                desired.insert(c);
            }
        }
    }

    // LOD chunks to load: in desired but not tracked
    std::vector<ChunkCoord> toLoad;
    for (const auto& c : desired) {
        if (!lodChunks_.contains(c))
            toLoad.push_back(c);
    }

    // Sort by distance (nearest first)
    auto distSq = [&](const ChunkCoord& c) {
        int ddx = c.cx - centerCX;
        int ddy = c.cy - centerCY;
        int ddz = c.cz - centerCZ;
        return ddx * ddx + ddy * ddy + ddz * ddz;
    };
    std::sort(toLoad.begin(), toLoad.end(),
              [&](const ChunkCoord& a, const ChunkCoord& b) { return distSq(a) < distSq(b); });

    // Rate-limit loads
    int loadCount = std::min(static_cast<int>(toLoad.size()), lodGenBudget_);
    for (int i = 0; i < loadCount; ++i) {
        const auto& c = toLoad[static_cast<size_t>(i)];
        if (lodSystem_)
            lodSystem_->requestDirectLOD(c.cx, c.cy, c.cz);
        lodChunks_.insert(c);
    }

    // LOD chunks to unload: entered full-res ring or left LOD ring.
    // Hysteresis prevents thrash at the boundary: load at lodRadius_,
    // unload at lodRadius_ + margin.
    constexpr int K_LOD_HYSTERESIS = 2;
    int unloadRadius = lodRadius_ + K_LOD_HYSTERESIS;

    std::vector<ChunkCoord> toUnload;
    for (const auto& c : lodChunks_) {
        int ddx = c.cx - centerCX;
        int ddy = c.cy - centerCY;
        int ddz = c.cz - centerCZ;
        bool insideFullRes =
            std::abs(ddx) <= chunkRadius && std::abs(ddy) <= chunkRadius && std::abs(ddz) <= chunkRadius;
        bool beyondUnload =
            std::abs(ddx) > unloadRadius || std::abs(ddy) > unloadRadius || std::abs(ddz) > unloadRadius;
        if (insideFullRes || beyondUnload)
            toUnload.push_back(c);
    }
    for (const auto& c : toUnload) {
        if (lodSystem_) {
            int ddx = c.cx - centerCX;
            int ddy = c.cy - centerCY;
            int ddz = c.cz - centerCZ;
            bool insideFullRes =
                std::abs(ddx) <= chunkRadius && std::abs(ddy) <= chunkRadius && std::abs(ddz) <= chunkRadius;
            if (insideFullRes) {
                // Chunk entered full-res ring; keep section data for LOD cascade
                lodSystem_->onChunkRemoved(c.cx, c.cy, c.cz);
            } else {
                // Chunk left LOD ring entirely; free GPU + grid data
                lodSystem_->removeSectionFully(c.cx, c.cy, c.cz);
            }
        }
        lodChunks_.erase(c);
    }
}

void ChunkPipelineSystem::configureDependencies() {
    after<TerrainSystem>();
}

ChunkPipelineDebugInfo ChunkPipelineSystem::debugInfo() const {
    ChunkPipelineDebugInfo info;
    info.trackedChunks = static_cast<int>(chunkEntities_.size());
    info.chunksLoadedThisFrame = loadsThisFrame_;
    info.chunksUnloadedThisFrame = unloadsThisFrame_;
    info.currentStreamingRadius = streaming_ ? streaming_->currentRadius() : 0.0f;
    return info;
}

bool ChunkPipelineSystem::tryLoadChunkFromDisk(int cx, int cy, int cz) {
    if (!chunkStore_ || !simSystem_)
        return false;

    // Check for gen data on disk
    if (!chunkStore_->hasGenData(cx, cy, cz))
        return false;

    auto genBlob = chunkStore_->loadGenData(cx, cy, cz);
    if (!genBlob)
        return false;

    auto& grid = simSystem_->simulationGrid();
    grid.registry().addChunk(cx, cy, cz);
    grid.registry().transitionState(cx, cy, cz, recurse::simulation::ChunkSlotState::Generating);

    // Decode FCHK and write into grid
    auto [payload, payloadSize] = FilesystemChunkStore::decodeView(*genBlob);
    auto* buf = grid.writeBuffer(cx, cy, cz);
    if (buf && payloadSize == sizeof(*buf)) {
        std::memcpy(buf->data(), payload, payloadSize);
    }

    // Apply delta if exists
    if (chunkStore_->hasDelta(cx, cy, cz)) {
        auto deltaBlob = chunkStore_->loadDelta(cx, cy, cz);
        if (deltaBlob) {
            auto [dp, ds] = FilesystemChunkStore::decodeView(*deltaBlob);
            if (buf && ds == sizeof(*buf)) {
                std::memcpy(buf->data(), dp, ds);
            }
        }
    }

    grid.syncChunkBuffers(cx, cy, cz);
    grid.registry().transitionState(cx, cy, cz, recurse::simulation::ChunkSlotState::Active);
    simSystem_->activityTracker().setState(recurse::simulation::ChunkPos{cx, cy, cz},
                                           recurse::simulation::ChunkState::Active);

    return true;
}

void ChunkPipelineSystem::saveChunkToDisk(int cx, int cy, int cz) {
    if (!saveService_ || !simSystem_)
        return;

    auto& grid = simSystem_->simulationGrid();
    const auto* buf = grid.readBuffer(cx, cy, cz);
    if (!buf)
        return;

    // Encode VoxelCell data to FCHK blob and queue for debounced save
    auto blob = FilesystemChunkStore::encode(buf->data(), sizeof(*buf));
    // Direct save on unload (no debounce; chunk is about to be destroyed)
    if (chunkStore_) {
        if (chunkStore_->hasGenData(cx, cy, cz)) {
            chunkStore_->saveDelta(cx, cy, cz, blob);
        } else {
            chunkStore_->saveGenData(cx, cy, cz, blob);
        }
    }
}

} // namespace recurse::systems
