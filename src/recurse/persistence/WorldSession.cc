#include "recurse/persistence/WorldSession.hh"

#include "recurse/character/VoxelInteraction.hh"
#include "recurse/components/StreamSource.hh"
#include "recurse/persistence/ChunkSaveService.hh"
#include "recurse/persistence/FchkCodec.hh"
#include "recurse/persistence/PruningScheduler.hh"
#include "recurse/persistence/SnapshotScheduler.hh"
#include "recurse/persistence/SqliteChunkStore.hh"
#include "recurse/persistence/SqliteTransactionStore.hh"
#include "recurse/simulation/ChunkActivityTracker.hh"
#include "recurse/simulation/SimulationGrid.hh"
#include "recurse/systems/LODSystem.hh"
#include "recurse/systems/PhysicsGameSystem.hh"
#include "recurse/systems/TerrainSystem.hh"
#include "recurse/systems/VoxelMeshingSystem.hh"
#include "recurse/systems/VoxelSimulationSystem.hh"
#include "recurse/world/WorldGenerator.hh"

#include "fabric/core/ECS.hh"
#include "fabric/core/Event.hh"
#include "fabric/core/Log.hh"
#include "fabric/platform/JobScheduler.hh"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>

namespace recurse {

// ---------------------------------------------------------------------------
// Static factory
// ---------------------------------------------------------------------------

fabric::fx::Result<std::unique_ptr<WorldSession>, fabric::fx::IOError>
WorldSession::open(const std::string& worldDir, fabric::EventDispatcher& dispatcher, fabric::JobScheduler& scheduler,
                   flecs::world& ecsWorld, systems::VoxelSimulationSystem* simSystem,
                   systems::VoxelMeshingSystem* meshingSystem, systems::LODSystem* lodSystem,
                   systems::PhysicsGameSystem* physicsSystem, systems::TerrainSystem* terrainSystem) {
    std::unique_ptr<SqliteChunkStore> store;
    try {
        store = std::make_unique<SqliteChunkStore>(worldDir);
    } catch (const std::exception& ex) {
        return fabric::fx::Result<std::unique_ptr<WorldSession>, fabric::fx::IOError>::failure(
            fabric::fx::IOError{worldDir, 0, {ex.what()}});
    }

    auto session = std::unique_ptr<WorldSession>(new WorldSession(worldDir, dispatcher, scheduler, ecsWorld,
                                                                  std::move(store), simSystem, meshingSystem, lodSystem,
                                                                  physicsSystem, terrainSystem));

    return fabric::fx::Result<std::unique_ptr<WorldSession>, fabric::fx::IOError>::success(std::move(session));
}

// ---------------------------------------------------------------------------
// Private constructor
// ---------------------------------------------------------------------------

WorldSession::WorldSession(const std::string& worldDir, fabric::EventDispatcher& dispatcher,
                           fabric::JobScheduler& scheduler, flecs::world& ecsWorld,
                           std::unique_ptr<SqliteChunkStore> store, systems::VoxelSimulationSystem* simSystem,
                           systems::VoxelMeshingSystem* meshingSystem, systems::LODSystem* lodSystem,
                           systems::PhysicsGameSystem* physicsSystem, systems::TerrainSystem* terrainSystem)
    : store_(std::move(store)),
      dispatcher_(dispatcher),
      scheduler_(scheduler),
      simSystem_(simSystem),
      meshingSystem_(meshingSystem),
      lodSystem_(lodSystem),
      physicsSystem_(physicsSystem),
      terrainSystem_(terrainSystem) {

    txStore_ = std::make_unique<SqliteTransactionStore>(store_->writerDb(), store_->readerDb());

    auto provider = [this](int cx, int cy, int cz) -> ChunkBlob {
        return encodeChunkBlob(cx, cy, cz);
    };

    saveService_ = std::make_unique<ChunkSaveService>(*store_, writerQueue_, provider);
    snapshotScheduler_ = std::make_unique<SnapshotScheduler>(*txStore_, writerQueue_, provider);
    pruningScheduler_ = std::make_unique<PruningScheduler>(*txStore_, writerQueue_);

    // Subscribe to voxel change events for save/snapshot/transaction tracking.
    // Handler ID stored for destructor unsubscription (fixes K32).
    listenerHandlerId_ = dispatcher_.addEventListener(K_VOXEL_CHANGED_EVENT, [this](fabric::Event& e) {
        int cx = e.getData<int>("cx");
        int cy = e.getData<int>("cy");
        int cz = e.getData<int>("cz");

        if (saveService_)
            saveService_->markDirty(cx, cy, cz);
        if (snapshotScheduler_)
            snapshotScheduler_->markDirty(cx, cy, cz);

        if (txStore_ && e.hasAnyData("detail")) {
            auto details = e.getAnyData<std::vector<VoxelChangeDetail>>("detail");
            if (!details.empty()) {
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
                    pendingChanges_.push_back(vc);
                }
            }
        }
    });

    streamSourceQuery_ = ecsWorld.query_builder<const fabric::Position, const StreamSource>().build();

    FABRIC_LOG_INFO("WorldSession opened for {}", worldDir);
}

// ---------------------------------------------------------------------------
// Destructor (ordered teardown)
// ---------------------------------------------------------------------------

WorldSession::~WorldSession() {
    // Step 0: Unsubscribe event listener to prevent callbacks during teardown
    dispatcher_.removeEventListener(K_VOXEL_CHANGED_EVENT, listenerHandlerId_);

    // Step 1: Cancel and drain all pending async load futures
    for (auto& pl : pendingLoads_)
        pl.result.wait();
    pendingLoads_.clear();

    // Step 2: Flush snapshot scheduler
    if (snapshotScheduler_)
        snapshotScheduler_->flush();

    // Step 3: Flush save service
    if (saveService_)
        saveService_->flush();

    // Step 4: Flush transaction store (currently no-op)
    if (txStore_)
        txStore_->flush();

    // Step 5: Drain WriterQueue so all writer I/O completes before service destruction
    writerQueue_.drain();

    // Steps 6-8: unique_ptrs destroy in reverse declaration order
    pruningScheduler_.reset();
    snapshotScheduler_.reset();
    saveService_.reset();
    txStore_.reset();

    // Step 9: Clear per-world ECS state
    for (auto& [_, entity] : chunkEntities_)
        entity.destruct();
    chunkEntities_.clear();
    streamSourceQuery_.reset();

    // Step 10: Clear LOD state
    lodChunks_.clear();
    lastLodCX_ = INT_MIN;
    lastLodCY_ = INT_MIN;
    lastLodCZ_ = INT_MIN;

    // Step 11: SqliteChunkStore destroyed by unique_ptr (declaration order; last owned resource)

    FABRIC_LOG_INFO("WorldSession closed");
}

// ---------------------------------------------------------------------------
// Migrated methods: encodeChunkBlob
// ---------------------------------------------------------------------------

ChunkBlob WorldSession::encodeChunkBlob(int cx, int cy, int cz) {
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

// ---------------------------------------------------------------------------
// Migrated methods: async chunk loading
// ---------------------------------------------------------------------------

bool WorldSession::dispatchAsyncLoad(int cx, int cy, int cz) {
    if (!store_ || !simSystem_)
        return false;

    if (!store_->hasChunk(cx, cy, cz))
        return false;

    auto& grid = simSystem_->simulationGrid();
    grid.registry().addChunk(cx, cy, cz);
    grid.registry().transitionState(cx, cy, cz, recurse::simulation::ChunkSlotState::Generating);

    int bufIdx = grid.currentWriteIndex();
    auto* buf = grid.writeBuffer(cx, cy, cz);
    if (!buf) {
        simSystem_->removeChunk(cx, cy, cz);
        return false;
    }

    auto* storePtr = store_.get();
    auto future = scheduler_.submit([storePtr, cx, cy, cz, buf]() -> AsyncLoadResult {
        AsyncLoadResult r;
        auto blob = storePtr->loadChunk(cx, cy, cz);
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

    pendingLoads_.push_back({std::move(future), cx, cy, cz, bufIdx, false});
    return true;
}

void WorldSession::pollPendingLoads(flecs::world& ecsWorld) {
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
            grid.syncChunkBuffersFrom(cx, cy, cz, it->bufferIndex);
            grid.registry().transitionState(cx, cy, cz, recurse::simulation::ChunkSlotState::Active);
            simSystem_->activityTracker().setState(fabric::ChunkCoord{cx, cy, cz},
                                                   recurse::simulation::ChunkState::Active);

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

            if (physicsSystem_)
                physicsSystem_->insertDirtyChunk(cx, cy, cz);

            fabric::ChunkCoord coord{cx, cy, cz};
            auto ent = ecsWorld.entity().add<fabric::SceneEntity>().set<fabric::BoundingBox>(
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

bool WorldSession::cancelPendingLoad(int cx, int cy, int cz) {
    for (auto& pl : pendingLoads_) {
        if (pl.cx == cx && pl.cy == cy && pl.cz == cz) {
            pl.cancelled = true;
            return true;
        }
    }
    return false;
}

bool WorldSession::hasPendingLoad(int cx, int cy, int cz) const {
    for (const auto& pl : pendingLoads_) {
        if (pl.cx == cx && pl.cy == cy && pl.cz == cz)
            return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// Migrated methods: LOD ring
// ---------------------------------------------------------------------------

void WorldSession::updateLODRing(int centerCX, int centerCY, int centerCZ, int streamingRadius, int lodRadius,
                                 int lodGenBudget) {
    if (centerCX == lastLodCX_ && centerCY == lastLodCY_ && centerCZ == lastLodCZ_)
        return;

    int chunkRadius = streamingRadius;

    int dx = centerCX - lastLodCX_;
    int dy = centerCY - lastLodCY_;
    int dz = centerCZ - lastLodCZ_;
    bool isTeleport = lastLodCX_ == INT_MIN || std::abs(dx) > 1 || std::abs(dy) > 1 || std::abs(dz) > 1;

    lastLodCX_ = centerCX;
    lastLodCY_ = centerCY;
    lastLodCZ_ = centerCZ;

    auto isLodCandidate = [&](const fabric::ChunkCoord& c) -> bool {
        int ddx = c.x - centerCX;
        int ddy = c.y - centerCY;
        int ddz = c.z - centerCZ;
        if (std::abs(ddx) <= chunkRadius && std::abs(ddy) <= chunkRadius && std::abs(ddz) <= chunkRadius)
            return false;
        if (terrainSystem_) {
            int surfaceMax = terrainSystem_->worldGenerator().maxSurfaceHeight(c.x, c.z);
            int chunkBottomY = c.y * K_CHUNK_SIZE;
            int chunkTopY = (c.y + 1) * K_CHUNK_SIZE;
            if (chunkBottomY > surfaceMax || chunkTopY < surfaceMax - K_CHUNK_SIZE)
                return false;
        }
        return true;
    };

    std::vector<fabric::ChunkCoord> toLoad;

    if (isTeleport) {
        for (int ddz = -lodRadius; ddz <= lodRadius; ++ddz)
            for (int ddy = -lodRadius; ddy <= lodRadius; ++ddy)
                for (int ddx = -lodRadius; ddx <= lodRadius; ++ddx) {
                    fabric::ChunkCoord c{centerCX + ddx, centerCY + ddy, centerCZ + ddz};
                    if (isLodCandidate(c) && !lodChunks_.contains(c))
                        toLoad.push_back(c);
                }
    } else {
        auto scanSlab = [&](int axis, int sign) {
            int edge = sign > 0 ? lodRadius : -lodRadius;
            for (int a = -lodRadius; a <= lodRadius; ++a) {
                for (int b = -lodRadius; b <= lodRadius; ++b) {
                    fabric::ChunkCoord c{};
                    if (axis == 0)
                        c = {centerCX + edge, centerCY + a, centerCZ + b};
                    else if (axis == 1)
                        c = {centerCX + a, centerCY + edge, centerCZ + b};
                    else
                        c = {centerCX + a, centerCY + b, centerCZ + edge};
                    if (isLodCandidate(c) && !lodChunks_.contains(c))
                        toLoad.push_back(c);
                }
            }
        };
        if (dx != 0)
            scanSlab(0, dx);
        if (dy != 0)
            scanSlab(1, dy);
        if (dz != 0)
            scanSlab(2, dz);
    }

    auto distSq = [&](const fabric::ChunkCoord& c) {
        int ddx = c.x - centerCX;
        int ddy = c.y - centerCY;
        int ddz = c.z - centerCZ;
        return ddx * ddx + ddy * ddy + ddz * ddz;
    };
    std::sort(toLoad.begin(), toLoad.end(),
              [&](const fabric::ChunkCoord& a, const fabric::ChunkCoord& b) { return distSq(a) < distSq(b); });

    int loadCount = std::min(static_cast<int>(toLoad.size()), lodGenBudget);
    for (int i = 0; i < loadCount; ++i) {
        const auto& c = toLoad[static_cast<size_t>(i)];
        if (lodSystem_)
            lodSystem_->requestDirectLOD(c.x, c.y, c.z);
        lodChunks_.insert(c);
    }

    constexpr int K_LOD_HYSTERESIS = 2;
    int unloadRadius = lodRadius + K_LOD_HYSTERESIS;

    std::vector<fabric::ChunkCoord> toUnload;
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
            if (insideFullRes)
                lodSystem_->onChunkRemoved(c.x, c.y, c.z);
            else
                lodSystem_->removeSectionFully(c.x, c.y, c.z);
        }
        lodChunks_.erase(c);
    }
}

// ---------------------------------------------------------------------------
// Session lifecycle methods
// ---------------------------------------------------------------------------

void WorldSession::bufferVoxelChange(const VoxelChange& change) {
    pendingChanges_.push_back(change);
}

void WorldSession::flushPendingChanges() {
    if (pendingChanges_.empty() || !txStore_)
        return;

    writerQueue_.submit([this, changes = std::move(pendingChanges_)]() { txStore_->logChanges(changes); });
    pendingChanges_.clear();
}

void WorldSession::updateSaveService(float dt) {
    if (saveService_)
        saveService_->update(dt);
}

void WorldSession::updateSnapshotScheduler(float dt) {
    if (snapshotScheduler_)
        snapshotScheduler_->update(dt);
}

void WorldSession::updatePruningScheduler(float dt) {
    if (pruningScheduler_)
        pruningScheduler_->update(dt);
}

// ---------------------------------------------------------------------------
// Accessors
// ---------------------------------------------------------------------------

SqliteChunkStore* WorldSession::chunkStore() const {
    return store_.get();
}

ChunkSaveService* WorldSession::saveService() const {
    return saveService_.get();
}

WorldTransactionStore* WorldSession::transactionStore() const {
    return txStore_.get();
}

} // namespace recurse
