#include "recurse/systems/ChunkPipelineSystem.hh"
#include "recurse/character/GameConstants.hh"
#include "recurse/components/StreamSource.hh"
#include "recurse/persistence/ChunkSaveService.hh"
#include "recurse/persistence/SqliteChunkStore.hh"
#include "recurse/persistence/WorldSession.hh"
#include "recurse/simulation/ChunkState.hh"
#include "recurse/simulation/SimulationGrid.hh"
#include "recurse/systems/CharacterMovementSystem.hh"
#include "recurse/systems/LODSystem.hh"
#include "recurse/systems/PhysicsGameSystem.hh"
#include "recurse/systems/TerrainSystem.hh"
#include "recurse/systems/VoxelMeshingSystem.hh"
#include "recurse/systems/VoxelSimulationSystem.hh"

#include "recurse/config/RecurseConfig.hh"

#include "fabric/core/AppContext.hh"
#include "fabric/core/SystemRegistry.hh"
#include "fabric/core/WorldLifecycle.hh"
#include "fabric/ecs/ECS.hh"
#include "fabric/log/Log.hh"
#include "fabric/platform/ConfigManager.hh"
#include "fabric/platform/JobScheduler.hh"
#include "fabric/utils/Profiler.hh"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <tuple>
#include <vector>

namespace recurse::systems {

ChunkPipelineSystem::ChunkPipelineSystem() = default;

ChunkPipelineSystem::~ChunkPipelineSystem() {
    unloadWorld();
}

void ChunkPipelineSystem::doInit(fabric::AppContext& ctx) {
    if (auto* wl = ctx.worldLifecycle) {
        wl->registerParticipant([this]() { onWorldBegin(); }, [this]() { onWorldEnd(); });
    }
    lodSystem_ = ctx.systemRegistry.get<LODSystem>();
    terrain_ = ctx.systemRegistry.get<TerrainSystem>();
    meshingSystem_ = ctx.systemRegistry.get<VoxelMeshingSystem>();
    simSystem_ = ctx.systemRegistry.get<VoxelSimulationSystem>();
    physics_ = ctx.systemRegistry.get<PhysicsGameSystem>();
    charMovement_ = ctx.systemRegistry.get<CharacterMovementSystem>();

    dispatcher_ = &ctx.dispatcher;
    ecsWorld_ = &ctx.world.get();

    StreamingConfig streamConfig;
    streamConfig.baseRadius = ctx.configManager.get<int>("terrain.chunk_radius", 8);
    streamConfig.maxLoadsPerTick = 64;
    streamConfig.maxUnloadsPerTick = 32;
    streamConfig.maxTrackedChunks = 4096;
    streamConfig.targetHighMs =
        ctx.configManager.get<float>("streaming.target_high_ms", recurse::RecurseConfig::K_DEFAULT_TARGET_HIGH_MS);
    streamConfig.targetLowMs =
        ctx.configManager.get<float>("streaming.target_low_ms", recurse::RecurseConfig::K_DEFAULT_TARGET_LOW_MS);
    streamConfig.floor =
        ctx.configManager.get<int>("streaming.floor", recurse::RecurseConfig::K_DEFAULT_STREAMING_FLOOR);
    streaming_ = std::make_unique<ChunkStreamingManager>(streamConfig);

    lodRadius_ = ctx.configManager.get<int>("lod.radius", 24);
    lodGenBudget_ = ctx.configManager.get<int>("lod.gen_budget", 4);

    maxAsyncLoadsPerFrame_ =
        ctx.configManager.get<int>("pipeline.max_async_loads", recurse::RecurseConfig::K_DEFAULT_MAX_ASYNC_LOADS);
    maxGeneratesPerFrame_ =
        ctx.configManager.get<int>("pipeline.max_generates", recurse::RecurseConfig::K_DEFAULT_MAX_GENERATES);
    maxLoadCompletions_ = ctx.configManager.get<int>("pipeline.max_load_completions",
                                                     recurse::RecurseConfig::K_DEFAULT_MAX_LOAD_COMPLETIONS);
    lodHysteresis_ =
        ctx.configManager.get<int>("pipeline.lod_hysteresis", recurse::RecurseConfig::K_DEFAULT_LOD_HYSTERESIS);
    collisionRadius_ =
        ctx.configManager.get<int>("physics.collision_radius", recurse::RecurseConfig::K_DEFAULT_COLLISION_RADIUS);
}

void ChunkPipelineSystem::doShutdown() {
    unloadWorld();
}

void ChunkPipelineSystem::fixedUpdate(fabric::AppContext& ctx, float fixedDt) {
    FABRIC_ZONE_SCOPED_N("chunk_pipeline");
    auto& ecsWorld = ctx.world;

    auto now = std::chrono::steady_clock::now();
    if (lastFrameTime_ != std::chrono::steady_clock::time_point{}) {
        float dtMs = std::chrono::duration<float, std::milli>(now - lastFrameTime_).count();
        if (dtMs > 1.0f)
            frameTimeEma_ = frameTimeEma_ * 0.9f + dtMs * 0.1f;
    }
    lastFrameTime_ = now;

    if (streaming_)
        streaming_->updateBudget(frameTimeEma_);

    if (!session_)
        return;

    auto loadCompletions = session_->pollPendingLoads();
    for (const auto& cl : loadCompletions) {
        if (!cl.success)
            continue;

        if (lodSystem_)
            lodSystem_->onChunkReady(cl.cx, cl.cy, cl.cz);

        if (physics_)
            physics_->insertDirtyChunk(cl.cx, cl.cy, cl.cz);

        fabric::ChunkCoord coord{cl.cx, cl.cy, cl.cz};
        auto ent = ecsWorld.get().entity().add<fabric::SceneEntity>().set<fabric::BoundingBox>(
            {static_cast<float>(cl.cx * K_CHUNK_SIZE), static_cast<float>(cl.cy * K_CHUNK_SIZE),
             static_cast<float>(cl.cz * K_CHUNK_SIZE), static_cast<float>((cl.cx + 1) * K_CHUNK_SIZE),
             static_cast<float>((cl.cy + 1) * K_CHUNK_SIZE), static_cast<float>((cl.cz + 1) * K_CHUNK_SIZE)});
        session_->chunkEntities()[coord] = ent;
    }

    // Collect focal points from StreamSource entities
    std::vector<FocalPoint> streamingFocals;
    std::vector<FocalPoint> collisionFocals;
    auto& query = session_->streamSourceQuery();
    if (query) {
        query->each([&](const fabric::Position& pos, const StreamSource& src) {
            if (src.streamRadius > 0)
                streamingFocals.push_back({pos.x, pos.y, pos.z, src.streamRadius});
            if (src.collisionRadius > 0)
                collisionFocals.push_back({pos.x, pos.y, pos.z, src.collisionRadius});
        });
    }

    if (streamingFocals.empty())
        streamingFocals.push_back({lastPlayerX_, lastPlayerY_, lastPlayerZ_, streaming_->config().baseRadius});

    auto streamUpdate = streaming_->update(streamingFocals);

    loadsThisFrame_ = static_cast<int>(streamUpdate.toLoad.size());
    unloadsThisFrame_ = static_cast<int>(streamUpdate.toUnload.size());

    // Triage new chunks: already-ready, async-loadable, or needs-generation.
    // Separate budgets prevent load-budget exhaustion from starving generation (K34).
    // Bounding-box pre-filter skips DB lookup for coords outside saved region (K35).
    int loadsDispatched = 0;
    int generatesQueued = 0;

    std::vector<ChunkCoord> readyChunks;
    std::vector<std::tuple<int, int, int>> toGenerate;
    size_t triageEnd = streamUpdate.toLoad.size();

    auto* chunkStore = session_->chunkStore();

    for (size_t i = 0; i < streamUpdate.toLoad.size(); ++i) {
        const auto& coord = streamUpdate.toLoad[i];

        if (session_->chunkEntities().find(coord) != session_->chunkEntities().end())
            continue;

        if (simSystem_ && simSystem_->simulationGrid().hasChunk(coord.x, coord.y, coord.z)) {
            if (recurse::simulation::findAs<recurse::simulation::Active>(simSystem_->simulationGrid().registry(),
                                                                         coord.x, coord.y, coord.z))
                readyChunks.push_back(coord);
            continue;
        }

        if (session_->hasPendingLoad(coord.x, coord.y, coord.z))
            continue;

        bool maybeInDb = chunkStore && chunkStore->isInSavedRegion(coord.x, coord.y, coord.z);

        if (maybeInDb && loadsDispatched < maxAsyncLoadsPerFrame_) {
            if (session_->dispatchAsyncLoad(coord.x, coord.y, coord.z)) {
                ++loadsDispatched;
                continue;
            }
        }

        if (generatesQueued < maxGeneratesPerFrame_) {
            toGenerate.emplace_back(coord.x, coord.y, coord.z);
            ++generatesQueued;
        } else {
            triageEnd = i;
            break;
        }
    }

    if (streaming_) {
        size_t deferred = streamUpdate.toLoad.size() - triageEnd;
        for (size_t i = triageEnd; i < streamUpdate.toLoad.size(); ++i)
            streaming_->untrack(streamUpdate.toLoad[i]);
        if (deferred > 0 || loadsDispatched > 0 || !toGenerate.empty()) {
            FABRIC_LOG_DEBUG("triage: load={} dbLoads={} gen={} ready={} deferred={} unload={}",
                             streamUpdate.toLoad.size(), loadsDispatched, toGenerate.size(), readyChunks.size(),
                             deferred, streamUpdate.toUnload.size());
        }
    }

    if (!toGenerate.empty() && simSystem_)
        simSystem_->generateChunksBatch(toGenerate);

    // Generated chunks are Active after batch gen (synchronous parallelFor)
    for (const auto& [cx, cy, cz] : toGenerate)
        readyChunks.push_back({cx, cy, cz});

    // Create ECS entities for chunks ready this frame
    for (const auto& coord : readyChunks) {
        if (lodSystem_)
            lodSystem_->onChunkReady(coord.x, coord.y, coord.z);

        auto ent = ecsWorld.get().entity().add<fabric::SceneEntity>().set<fabric::BoundingBox>(
            {static_cast<float>(coord.x * K_CHUNK_SIZE), static_cast<float>(coord.y * K_CHUNK_SIZE),
             static_cast<float>(coord.z * K_CHUNK_SIZE), static_cast<float>((coord.x + 1) * K_CHUNK_SIZE),
             static_cast<float>((coord.y + 1) * K_CHUNK_SIZE), static_cast<float>((coord.z + 1) * K_CHUNK_SIZE)});
        session_->chunkEntities()[coord] = ent;
    }

    // Unload
    for (const auto& coord : streamUpdate.toUnload) {
        // If async load in progress, cancel it. The background thread still holds
        // a pointer to the write buffer, so defer registry removal to pollPendingLoads.
        if (session_->cancelPendingLoad(coord.x, coord.y, coord.z))
            continue;

        auto blob = session_->encodeChunkBlob(coord.x, coord.y, coord.z);
        if (!blob.empty())
            session_->saveService()->enqueuePrepared(coord.x, coord.y, coord.z, std::move(blob));

        if (meshingSystem_)
            meshingSystem_->removeChunkMesh(coord);

        if (lodSystem_)
            lodSystem_->onChunkRemoved(coord.x, coord.y, coord.z);

        if (simSystem_)
            simSystem_->removeChunk(coord.x, coord.y, coord.z);

        if (physics_) {
            physics_->removeDirtyChunk(coord.x, coord.y, coord.z);
            physics_->physicsWorld().removeChunkCollision(coord.x, coord.y, coord.z);
        }

        auto& entities = session_->chunkEntities();
        if (auto it = entities.find(coord); it != entities.end()) {
            it->second.destruct();
            entities.erase(it);
        }
    }

    if (charMovement_) {
        const auto& pos = charMovement_->playerPosition();
        lastPlayerX_ = pos.x;
        lastPlayerY_ = pos.y;
        lastPlayerZ_ = pos.z;
    }

    if (physics_) {
        if (collisionFocals.empty())
            collisionFocals.push_back({lastPlayerX_, lastPlayerY_, lastPlayerZ_, collisionRadius_});
        physics_->setFocalPoints(collisionFocals);
    }

    session_->flushPendingChanges();
    session_->updateSaveService(fixedDt);
    session_->updateSnapshotScheduler(fixedDt);
    session_->updatePruningScheduler(fixedDt);

    int chunkRadius = streaming_ ? streaming_->currentRadius() : 0;
    if (lodRadius_ > chunkRadius) {
        int cx = static_cast<int>(std::floor(lastPlayerX_ / static_cast<float>(K_CHUNK_SIZE)));
        int cy = static_cast<int>(std::floor(lastPlayerY_ / static_cast<float>(K_CHUNK_SIZE)));
        int cz = static_cast<int>(std::floor(lastPlayerZ_ / static_cast<float>(K_CHUNK_SIZE)));
        session_->updateLODRing(cx, cy, cz, chunkRadius, lodRadius_, lodGenBudget_);
    }
}

void ChunkPipelineSystem::onWorldBegin() {
    lastFrameTime_ = std::chrono::steady_clock::now();
    frameTimeEma_ = 16.0f;
    lastPlayerX_ = K_DEFAULT_SPAWN_X;
    lastPlayerY_ = K_DEFAULT_SPAWN_Y;
    lastPlayerZ_ = K_DEFAULT_SPAWN_Z;
}

void ChunkPipelineSystem::onWorldEnd() {
    // WorldSession RAII handles heavy teardown; these are transient timing fields.
}

void ChunkPipelineSystem::configureDependencies() {
    after<TerrainSystem>();
}

ChunkPipelineDebugInfo ChunkPipelineSystem::debugInfo() const {
    ChunkPipelineDebugInfo info;
    info.trackedChunks = session_ ? static_cast<int>(session_->chunkEntities().size()) : 0;
    info.chunksLoadedThisFrame = loadsThisFrame_;
    info.chunksUnloadedThisFrame = unloadsThisFrame_;
    info.currentStreamingRadius = streaming_ ? streaming_->currentRadius() : 0.0f;
    return info;
}

const std::unordered_map<ChunkCoord, flecs::entity, ChunkCoordHash>& ChunkPipelineSystem::chunkEntities() const {
    static const std::unordered_map<ChunkCoord, flecs::entity, ChunkCoordHash> empty;
    return session_ ? session_->chunkEntities() : empty;
}

void ChunkPipelineSystem::loadWorld(const std::string& worldDir, fabric::JobScheduler& scheduler) {
    unloadWorld();

    auto result = recurse::WorldSession::open(worldDir, *dispatcher_, scheduler, *ecsWorld_, simSystem_, meshingSystem_,
                                              lodSystem_, physics_, terrain_);

    if (result.isSuccess()) {
        session_ = std::move(result.value());
        session_->setMaxLoadCompletions(maxLoadCompletions_);
        session_->setLodHysteresis(lodHysteresis_);
        FABRIC_LOG_INFO("ChunkPipeline: world loaded");
    } else {
        auto& err = result.error<fabric::fx::IOError>();
        FABRIC_LOG_ERROR("ChunkPipeline: failed to load world {}: {}", worldDir, err.ctx.message);
    }
}

void ChunkPipelineSystem::unloadWorld() {
    session_.reset();

    if (streaming_)
        streaming_->clear();
}

} // namespace recurse::systems
