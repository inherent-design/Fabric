#include "recurse/systems/ChunkPipelineSystem.hh"
#include "recurse/character/GameConstants.hh"
#include "recurse/character/VoxelInteraction.hh"
#include "recurse/persistence/ChunkSaveService.hh"
#include "recurse/persistence/ChunkStore.hh"
#include "recurse/persistence/FchkCodec.hh"
#include "recurse/persistence/PruningScheduler.hh"
#include "recurse/persistence/SnapshotScheduler.hh"
#include "recurse/persistence/SqliteChunkStore.hh"
#include "recurse/persistence/SqliteTransactionStore.hh"
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
#include "fabric/platform/JobScheduler.hh"
#include "fabric/utils/Profiler.hh"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <tuple>
#include <vector>

namespace recurse::systems {

ChunkPipelineSystem::~ChunkPipelineSystem() {
    unloadWorld();
}

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
    streamConfig.maxLoadsPerTick = 64;
    streamConfig.maxUnloadsPerTick = 32;
    streamConfig.maxTrackedChunks = 4096;
    streaming_ = std::make_unique<ChunkStreamingManager>(streamConfig);

    lodRadius_ = ctx.configManager.get<int>("lod.radius", 24);
    lodGenBudget_ = ctx.configManager.get<int>("lod.gen_budget", 4);

    streamSourceQuery_ = ctx.world.get().query_builder<const fabric::Position, const recurse::StreamSource>().build();

    ctx.dispatcher.addEventListener(K_VOXEL_CHANGED_EVENT, [this](fabric::Event& e) {
        int cx = e.getData<int>("cx");
        int cy = e.getData<int>("cy");
        int cz = e.getData<int>("cz");

        if (saveService_)
            saveService_->markDirty(cx, cy, cz);
        if (ownedSnapshotScheduler_)
            ownedSnapshotScheduler_->markDirty(cx, cy, cz);

        if (transactionStore_ && e.hasAnyData("detail")) {
            auto details = e.getAnyData<std::vector<VoxelChangeDetail>>("detail");
            if (!details.empty()) {
                std::vector<VoxelChange> changes;
                changes.reserve(details.size());
                auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                               std::chrono::system_clock::now().time_since_epoch())
                               .count();
                for (const auto& d : details) {
                    VoxelChange vc{};
                    vc.timestamp = now;
                    vc.addr = {cx, cy, cz, d.vx, d.vy, d.vz};
                    vc.oldCell = d.oldCell;
                    vc.newCell = d.newCell;
                    vc.playerId = d.playerId;
                    vc.source = d.source;
                    changes.push_back(vc);
                }
                transactionStore_->logChanges(changes);
            }
        }
    });
}

void ChunkPipelineSystem::doShutdown() {
    streamSourceQuery_.reset();
    unloadWorld();
}

void ChunkPipelineSystem::fixedUpdate(fabric::AppContext& ctx, float fixedDt) {
    FABRIC_ZONE_SCOPED_N("chunk_pipeline");
    auto& ecsWorld = ctx.world;

    // Complete async loads from previous frames before processing new work
    pollPendingLoads(ctx);

    // Collect focal points from StreamSource entities
    std::vector<FocalPoint> streamingFocals;
    std::vector<FocalPoint> collisionFocals;
    if (streamSourceQuery_) {
        streamSourceQuery_->each([&](const fabric::Position& pos, const StreamSource& src) {
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
    // Budget caps prevent frame stalls when loading worlds with many saved chunks.
    // Chunks beyond the async budget are untracked so the streaming manager retries them.
    constexpr int K_MAX_ASYNC_DISPATCHES_PER_FRAME = 32;
    int asyncDispatched = 0;

    std::vector<ChunkCoord> readyChunks;
    std::vector<std::tuple<int, int, int>> toGenerate;
    size_t triageEnd = streamUpdate.toLoad.size();
    for (size_t i = 0; i < streamUpdate.toLoad.size(); ++i) {
        const auto& coord = streamUpdate.toLoad[i];

        if (chunkEntities_.find(coord) != chunkEntities_.end())
            continue;

        if (simSystem_ && simSystem_->simulationGrid().hasChunk(coord.x, coord.y, coord.z)) {
            auto* slot = simSystem_->simulationGrid().registry().find(coord.x, coord.y, coord.z);
            if (slot && slot->state == recurse::simulation::ChunkSlotState::Active)
                readyChunks.push_back(coord);
            continue;
        }

        if (hasPendingLoad(coord.x, coord.y, coord.z))
            continue;

        if (asyncDispatched < K_MAX_ASYNC_DISPATCHES_PER_FRAME) {
            if (dispatchAsyncLoad(coord.x, coord.y, coord.z)) {
                ++asyncDispatched;
                continue;
            }
            // Not in DB; fall through to generation
            toGenerate.emplace_back(coord.x, coord.y, coord.z);
        } else {
            // Async budget exhausted; untrack remaining so they retry next frame
            triageEnd = i;
            break;
        }
    }

    if (streaming_) {
        size_t deferred = streamUpdate.toLoad.size() - triageEnd;
        for (size_t i = triageEnd; i < streamUpdate.toLoad.size(); ++i)
            streaming_->untrack(streamUpdate.toLoad[i]);
        if (deferred > 0 || asyncDispatched > 0 || !toGenerate.empty()) {
            FABRIC_LOG_DEBUG("triage: load={} async={} gen={} ready={} deferred={} unload={}",
                             streamUpdate.toLoad.size(), asyncDispatched, toGenerate.size(), readyChunks.size(),
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
        chunkEntities_[coord] = ent;
    }

    // Unload
    for (const auto& coord : streamUpdate.toUnload) {
        // If async load in progress, cancel it. The background thread still holds
        // a pointer to the write buffer, so defer registry removal to pollPendingLoads.
        if (cancelPendingLoad(coord.x, coord.y, coord.z))
            continue;

        if (saveService_) {
            auto blob = encodeChunkBlob(coord.x, coord.y, coord.z);
            if (!blob.empty())
                saveService_->enqueuePrepared(coord.x, coord.y, coord.z, std::move(blob));
        } else {
            saveChunkToDisk(coord.x, coord.y, coord.z);
        }

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

        if (auto it = chunkEntities_.find(coord); it != chunkEntities_.end()) {
            it->second.destruct();
            chunkEntities_.erase(it);
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
            collisionFocals.push_back({lastPlayerX_, lastPlayerY_, lastPlayerZ_, K_COLLISION_RADIUS});
        physics_->setFocalPoints(collisionFocals);
    }

    if (saveService_)
        saveService_->update(fixedDt);
    if (ownedSnapshotScheduler_)
        ownedSnapshotScheduler_->update(fixedDt);
    if (ownedPruningScheduler_)
        ownedPruningScheduler_->update(fixedDt);

    int chunkRadius = streaming_ ? streaming_->currentRadius() : 0;
    if (lodRadius_ > chunkRadius) {
        int cx = static_cast<int>(std::floor(lastPlayerX_ / static_cast<float>(K_CHUNK_SIZE)));
        int cy = static_cast<int>(std::floor(lastPlayerY_ / static_cast<float>(K_CHUNK_SIZE)));
        int cz = static_cast<int>(std::floor(lastPlayerZ_ / static_cast<float>(K_CHUNK_SIZE)));
        updateLODRing(cx, cy, cz);
    }
}

void ChunkPipelineSystem::updateLODRing(int centerCX, int centerCY, int centerCZ) {
    FABRIC_ZONE_SCOPED_N("lod_ring_update");

    if (centerCX == lastLodCX_ && centerCY == lastLodCY_ && centerCZ == lastLodCZ_)
        return;
    lastLodCX_ = centerCX;
    lastLodCY_ = centerCY;
    lastLodCZ_ = centerCZ;

    int chunkRadius = streaming_ ? streaming_->currentRadius() : 0;

    std::unordered_set<ChunkCoord, ChunkCoordHash> desired;
    for (int dz = -lodRadius_; dz <= lodRadius_; ++dz) {
        for (int dy = -lodRadius_; dy <= lodRadius_; ++dy) {
            for (int dx = -lodRadius_; dx <= lodRadius_; ++dx) {
                if (std::abs(dx) <= chunkRadius && std::abs(dy) <= chunkRadius && std::abs(dz) <= chunkRadius)
                    continue;

                ChunkCoord c{centerCX + dx, centerCY + dy, centerCZ + dz};

                if (terrain_) {
                    int surfaceMax = terrain_->worldGenerator().maxSurfaceHeight(c.x, c.z);
                    int chunkBottomY = c.y * K_CHUNK_SIZE;
                    int chunkTopY = (c.y + 1) * K_CHUNK_SIZE;
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
        int ddx = c.x - centerCX;
        int ddy = c.y - centerCY;
        int ddz = c.z - centerCZ;
        return ddx * ddx + ddy * ddy + ddz * ddz;
    };
    std::sort(toLoad.begin(), toLoad.end(),
              [&](const ChunkCoord& a, const ChunkCoord& b) { return distSq(a) < distSq(b); });

    // Rate-limit loads
    int loadCount = std::min(static_cast<int>(toLoad.size()), lodGenBudget_);
    for (int i = 0; i < loadCount; ++i) {
        const auto& c = toLoad[static_cast<size_t>(i)];
        if (lodSystem_)
            lodSystem_->requestDirectLOD(c.x, c.y, c.z);
        lodChunks_.insert(c);
    }

    // LOD chunks to unload: entered full-res ring or left LOD ring.
    // Hysteresis prevents thrash at the boundary: load at lodRadius_,
    // unload at lodRadius_ + margin.
    constexpr int K_LOD_HYSTERESIS = 2;
    int unloadRadius = lodRadius_ + K_LOD_HYSTERESIS;

    std::vector<ChunkCoord> toUnload;
    for (const auto& c : lodChunks_) {
        int ddx = c.x - centerCX;
        int ddy = c.y - centerCY;
        int ddz = c.z - centerCZ;
        bool insideFullRes =
            std::abs(ddx) <= chunkRadius && std::abs(ddy) <= chunkRadius && std::abs(ddz) <= chunkRadius;
        bool beyondUnload =
            std::abs(ddx) > unloadRadius || std::abs(ddy) > unloadRadius || std::abs(ddz) > unloadRadius;
        if (insideFullRes || beyondUnload)
            toUnload.push_back(c);
    }
    for (const auto& c : toUnload) {
        if (lodSystem_) {
            int ddx = c.x - centerCX;
            int ddy = c.y - centerCY;
            int ddz = c.z - centerCZ;
            bool insideFullRes =
                std::abs(ddx) <= chunkRadius && std::abs(ddy) <= chunkRadius && std::abs(ddz) <= chunkRadius;
            if (insideFullRes) {
                // Chunk entered full-res ring; keep section data for LOD cascade
                lodSystem_->onChunkRemoved(c.x, c.y, c.z);
            } else {
                // Chunk left LOD ring entirely; free GPU + grid data
                lodSystem_->removeSectionFully(c.x, c.y, c.z);
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

bool ChunkPipelineSystem::dispatchAsyncLoad(int cx, int cy, int cz) {
    if (!chunkStore_ || !simSystem_)
        return false;

    if (!chunkStore_->hasChunk(cx, cy, cz))
        return false;

    auto& grid = simSystem_->simulationGrid();
    grid.registry().addChunk(cx, cy, cz);
    grid.registry().transitionState(cx, cy, cz, recurse::simulation::ChunkSlotState::Generating);

    // writeBuffer() auto-materializes the slot
    auto* buf = grid.writeBuffer(cx, cy, cz);
    if (!buf) {
        simSystem_->removeChunk(cx, cy, cz);
        return false;
    }

    auto* store = chunkStore_;
    using Result = ChunkPipelineSystem::AsyncLoadResult;
    auto future = simSystem_->scheduler().submit([store, cx, cy, cz, buf]() -> Result {
        Result r;
        auto blob = store->loadChunk(cx, cy, cz);
        if (!blob)
            return r;

        auto decoded = FchkCodec::decode(*blob);
        if (decoded.cells.size() == sizeof(*buf))
            std::memcpy(buf->data(), decoded.cells.data(), decoded.cells.size());

        r.paletteData = std::move(decoded.paletteData);
        r.paletteEntryCount = decoded.paletteEntryCount;

        r.success = true;
        return r;
    });

    pendingLoads_.push_back({std::move(future), cx, cy, cz, false});
    return true;
}

void ChunkPipelineSystem::pollPendingLoads(fabric::AppContext& ctx) {
    constexpr int K_MAX_LOAD_COMPLETIONS_PER_FRAME = 16;
    int completed = 0;

    auto it = pendingLoads_.begin();
    while (it != pendingLoads_.end()) {
        if (it->result.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
            ++it;
            continue;
        }

        auto loadResult = it->result.get();
        int cx = it->cx;
        int cy = it->cy;
        int cz = it->cz;

        if (it->cancelled) {
            if (simSystem_)
                simSystem_->removeChunk(cx, cy, cz);
            it = pendingLoads_.erase(it);
            continue;
        }

        if (loadResult.success) {
            auto& grid = simSystem_->simulationGrid();
            grid.syncChunkBuffers(cx, cy, cz);
            grid.registry().transitionState(cx, cy, cz, recurse::simulation::ChunkSlotState::Active);
            simSystem_->activityTracker().setState(fabric::ChunkCoord{cx, cy, cz},
                                                   recurse::simulation::ChunkState::Active);

            // Reconstruct palette from decoded FCHK data
            if (loadResult.paletteEntryCount > 0) {
                auto* palette = grid.chunkPalette(cx, cy, cz);
                if (palette) {
                    palette->clear();
                    for (uint16_t i = 0; i < loadResult.paletteEntryCount; ++i) {
                        size_t base = static_cast<size_t>(i) * 4;
                        palette->addEntry({loadResult.paletteData[base], loadResult.paletteData[base + 1],
                                           loadResult.paletteData[base + 2], loadResult.paletteData[base + 3]});
                    }
                }
            }

            if (lodSystem_)
                lodSystem_->onChunkReady(cx, cy, cz);

            if (physics_)
                physics_->insertDirtyChunk(cx, cy, cz);

            auto& ecsWorld = ctx.world;
            ChunkCoord coord{cx, cy, cz};
            auto ent = ecsWorld.get().entity().add<fabric::SceneEntity>().set<fabric::BoundingBox>(
                {static_cast<float>(cx * K_CHUNK_SIZE), static_cast<float>(cy * K_CHUNK_SIZE),
                 static_cast<float>(cz * K_CHUNK_SIZE), static_cast<float>((cx + 1) * K_CHUNK_SIZE),
                 static_cast<float>((cy + 1) * K_CHUNK_SIZE), static_cast<float>((cz + 1) * K_CHUNK_SIZE)});
            chunkEntities_[coord] = ent;
        } else {
            if (simSystem_)
                simSystem_->removeChunk(cx, cy, cz);
        }

        it = pendingLoads_.erase(it);
        if (++completed >= K_MAX_LOAD_COMPLETIONS_PER_FRAME)
            break;
    }
}

bool ChunkPipelineSystem::cancelPendingLoad(int cx, int cy, int cz) {
    for (auto& pl : pendingLoads_) {
        if (pl.cx == cx && pl.cy == cy && pl.cz == cz) {
            pl.cancelled = true;
            return true;
        }
    }
    return false;
}

bool ChunkPipelineSystem::hasPendingLoad(int cx, int cy, int cz) const {
    for (const auto& pl : pendingLoads_) {
        if (pl.cx == cx && pl.cy == cy && pl.cz == cz)
            return true;
    }
    return false;
}

void ChunkPipelineSystem::loadWorld(const std::string& worldDir, fabric::JobScheduler& scheduler) {
    unloadWorld();

    ownedStore_ = std::make_unique<SqliteChunkStore>(worldDir);

    auto* sqliteStore = static_cast<SqliteChunkStore*>(ownedStore_.get());
    ownedTransactionStore_ = std::make_unique<SqliteTransactionStore>(sqliteStore->writerDb(), sqliteStore->readerDb());

    auto provider = [this](int cx, int cy, int cz) -> ChunkBlob {
        return encodeChunkBlob(cx, cy, cz);
    };
    ownedSaveService_ = std::make_unique<ChunkSaveService>(*ownedStore_, scheduler, provider);
    ownedSnapshotScheduler_ = std::make_unique<SnapshotScheduler>(*ownedTransactionStore_, provider);
    ownedPruningScheduler_ = std::make_unique<PruningScheduler>(*ownedTransactionStore_);

    chunkStore_ = ownedStore_.get();
    saveService_ = ownedSaveService_.get();
    transactionStore_ = ownedTransactionStore_.get();

    FABRIC_LOG_INFO("ChunkPipeline: persistence wired for {}", worldDir);
}

void ChunkPipelineSystem::unloadWorld() {
    // Drain pending async loads first. Background futures hold raw pointers
    // to the store and write buffers; destroying those before the futures
    // complete is use-after-free.
    for (auto& pl : pendingLoads_)
        pl.result.wait();
    pendingLoads_.clear();

    // Flush persistence while grid data is still valid.
    if (ownedSnapshotScheduler_)
        ownedSnapshotScheduler_->flush();
    if (ownedSaveService_)
        ownedSaveService_->flush();
    if (ownedTransactionStore_)
        ownedTransactionStore_->flush();

    ownedSnapshotScheduler_.reset();
    ownedPruningScheduler_.reset();
    ownedSaveService_.reset();
    ownedTransactionStore_.reset();
    ownedStore_.reset();
    chunkStore_ = nullptr;
    saveService_ = nullptr;
    transactionStore_ = nullptr;

    // Destroy stale ECS entities so re-entry doesn't skip already-tracked coords.
    for (auto& [_, entity] : chunkEntities_)
        entity.destruct();
    chunkEntities_.clear();

    // Reset streaming so the manager re-discovers chunks on next update().
    if (streaming_)
        streaming_->clear();

    // Reset LOD ring state.
    lodChunks_.clear();
    lastLodCX_ = INT_MIN;
    lastLodCY_ = INT_MIN;
    lastLodCZ_ = INT_MIN;
}

ChunkBlob ChunkPipelineSystem::encodeChunkBlob(int cx, int cy, int cz) {
    if (!simSystem_)
        return {};

    auto& grid = simSystem_->simulationGrid();
    const auto* buf = grid.readBuffer(cx, cy, cz);
    if (!buf)
        return {};

    const float* palettePtr = nullptr;
    uint16_t paletteCount = 0;
    std::vector<float> paletteFloats;
    const auto* palette = grid.chunkPalette(cx, cy, cz);
    if (palette && palette->paletteSize() > 0) {
        paletteCount = static_cast<uint16_t>(palette->paletteSize());
        paletteFloats.resize(static_cast<size_t>(paletteCount) * 4);
        for (uint16_t i = 0; i < paletteCount; ++i) {
            auto e = palette->lookup(i);
            size_t base = static_cast<size_t>(i) * 4;
            paletteFloats[base] = e.x;
            paletteFloats[base + 1] = e.y;
            paletteFloats[base + 2] = e.z;
            paletteFloats[base + 3] = e.w;
        }
        palettePtr = paletteFloats.data();
    }

    return FchkCodec::encode(buf->data(), sizeof(*buf), 1, 1, palettePtr, paletteCount);
}

void ChunkPipelineSystem::saveChunkToDisk(int cx, int cy, int cz) {
    if (!chunkStore_)
        return;

    auto blob = encodeChunkBlob(cx, cy, cz);
    if (blob.empty())
        return;

    chunkStore_->saveChunk(cx, cy, cz, blob);
}

} // namespace recurse::systems
