#include "recurse/persistence/WorldSession.hh"

#include "recurse/character/VoxelInteraction.hh"
#include "recurse/components/StreamSource.hh"
#include "recurse/persistence/ChunkSaveService.hh"
#include "recurse/persistence/ChunkSnapshotProvider.hh"
#include "recurse/persistence/FchkCodec.hh"
#include "recurse/persistence/PruningScheduler.hh"
#include "recurse/persistence/ReplayExecutor.hh"
#include "recurse/persistence/SnapshotScheduler.hh"
#include "recurse/persistence/SqliteChunkStore.hh"
#include "recurse/persistence/SqliteTransactionStore.hh"
#include "recurse/simulation/ChunkActivityTracker.hh"
#include "recurse/simulation/ChunkFinalization.hh"
#include "recurse/simulation/ChunkState.hh"
#include "recurse/simulation/SimulationGrid.hh"
#include "recurse/systems/LODSystem.hh"
#include "recurse/systems/PhysicsGameSystem.hh"
#include "recurse/systems/TerrainSystem.hh"
#include "recurse/systems/VoxelMeshingSystem.hh"
#include "recurse/systems/VoxelSimulationSystem.hh"
#include "recurse/world/WorldGenerator.hh"

#include "fabric/core/Event.hh"
#include "fabric/ecs/ECS.hh"
#include "fabric/log/Log.hh"
#include "fabric/platform/JobScheduler.hh"

#include "recurse/simulation/EssenceAssigner.hh"
#include "recurse/simulation/MaterialRegistry.hh"
#include "recurse/simulation/VoxelConstants.hh"
#include "recurse/simulation/VoxelMaterial.hh"
#include "recurse/world/EssencePalette.hh"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstring>
#include <memory>

inline constexpr float K_CHECKPOINT_INTERVAL_SECONDS = 5.0f;

namespace recurse {

namespace {
uint32_t computeWorldgenVersion(const std::string& genName, int64_t worldSeed) {
    uint32_t h = 2166136261u;
    for (char c : genName) {
        h ^= static_cast<uint32_t>(static_cast<uint8_t>(c));
        h *= 16777619u;
    }
    auto seed = static_cast<uint64_t>(worldSeed);
    for (int i = 0; i < 8; ++i) {
        h ^= static_cast<uint32_t>((seed >> (i * 8)) & 0xFF);
        h *= 16777619u;
    }
    return h;
}
} // namespace

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
      worldDir_(worldDir),
      dispatcher_(dispatcher),
      scheduler_(scheduler),
      ecsWorld_(ecsWorld),
      simSystem_(simSystem),
      meshingSystem_(meshingSystem),
      lodSystem_(lodSystem),
      physicsSystem_(physicsSystem),
      terrainSystem_(terrainSystem) {

    txStore_ = std::make_unique<SqliteTransactionStore>(store_->writerDb(), store_->readerDb());

    if (terrainSystem_) {
        worldGen_ = &terrainSystem_->worldGenerator();
        worldgenVersion_ = computeWorldgenVersion(worldGen_->name(), simSystem_ ? simSystem_->worldSeed() : 0);
    }

    if (worldgenVersion_ != 0)
        store_->setWorldgenVersion(worldgenVersion_);

    auto provider = [this](int cx, int cy, int cz) -> ChunkBlob {
        return encodeChunkBlob(cx, cy, cz);
    };

    saveService_ = std::make_unique<ChunkSaveService>(*store_, writerQueue_, provider);
    snapshotScheduler_ = std::make_unique<SnapshotScheduler>(*txStore_, writerQueue_, provider);
    pruningScheduler_ = std::make_unique<PruningScheduler>(*txStore_, writerQueue_);

    if (simSystem_) {
        replayExecutor_ = std::make_unique<persistence::ReplayExecutor>(
            *txStore_, simSystem_->simulationGrid(), simSystem_->fallingSandSystem(), simSystem_->ghostCellManager(),
            simSystem_->activityTracker(), simSystem_->worldSeed(), &simSystem_->materials(), worldGen_);

        chunkSnapshotProvider_ = std::make_unique<ChunkSnapshotProvider>(
            *txStore_, *snapshotScheduler_, simSystem_->simulationGrid(), *replayExecutor_);
    }

    // Subscribe to voxel change events for save/snapshot/transaction tracking.
    // Handler ID stored for destructor unsubscription (fixes K32).
    listenerHandlerId_ = dispatcher_.addEventListener(K_VOXEL_CHANGED_EVENT, [this](fabric::Event& e) {
        int cx = e.getData<int>("cx");
        int cy = e.getData<int>("cy");
        int cz = e.getData<int>("cz");

        // F22: Filter events that cannot produce meaningful persistence diffs.
        if (e.hasData("source")) {
            auto src = static_cast<ChangeSource>(e.getData<int>("source"));
            if (src == ChangeSource::Generation)
                return; // fresh worldgen matches reference; zero diff by definition
            if (src == ChangeSource::Physics && !e.hasAnyData("detail"))
                return; // settled-chunk event (collision-only, no cell changes)
        }

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

    FABRIC_LOG_INFO("WorldSession: opened dir='{}'", worldDir_);
}

// ---------------------------------------------------------------------------
// Destructor (ordered teardown)
// ---------------------------------------------------------------------------

WorldSession::~WorldSession() {
    auto status = runtimeStatusSnapshot();
    const char* errorText = status.saveActivity.hasError ? status.saveActivity.lastError.c_str() : "none";
    FABRIC_LOG_INFO("WorldSession: teardown start dir='{}' pendingLoads={} dirty={} saving={} prepared={} lastSave={} "
                    "error={}",
                    worldDir_, status.pendingLoads, status.saveActivity.dirtyChunks, status.saveActivity.savingChunks,
                    status.saveActivity.preparedChunks, status.saveActivity.lastSuccessfulSerial, errorText);

    // Step 0: Unsubscribe event listener to prevent callbacks during teardown
    dispatcher_.removeEventListener(K_VOXEL_CHANGED_EVENT, listenerHandlerId_);

    // Step 1: Cancel pending async loads (ScopedTaskGroup destructor drains futures)
    pendingLoads_.cancelAll();

    // Step 2: Flush snapshot scheduler
    if (snapshotScheduler_)
        snapshotScheduler_->flush();

    // Step 2b: Final prune pass before shutdown
    if (pruningScheduler_)
        pruningScheduler_->pruneNow();

    // Step 2c: Persist any resident active chunks that were never streamed out
    // or otherwise enqueued. This closes the close/reopen bootstrap gap without
    // re-enabling eager generation dirty tracking.
    enqueueResidentChunksForShutdown();

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
    chunkSnapshotProvider_.reset();
    replayExecutor_.reset();
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

    FABRIC_LOG_INFO("WorldSession: teardown complete dir='{}'", worldDir_);
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

    if (worldGen_) {
        std::vector<simulation::VoxelCell> refBuf(simulation::K_CHUNK_VOLUME);
        worldGen_->generateToBuffer(refBuf.data(), cx, cy, cz);
        EssencePalette refPalette;
        simulation::assignEssence(refBuf.data(), cx, cy, cz, simSystem_->materials(), refPalette, 42);
        auto blob = FchkCodec::encodeDelta(buf->data(), refBuf.data(), sizeof(*buf), worldgenVersion_, 1, 1, palettePtr,
                                           paletteCount);
        ++stats_.encodes;
        stats_.encodeBytes += blob.size();
        // Heuristic: empty delta compresses to ~29 bytes (header + zero diffs + empty palette)
        if (blob.size() < 40)
            ++stats_.zeroDiffEncodes;
        return blob;
    }

    auto blob = FchkCodec::encode(buf->data(), sizeof(*buf), 1, 1, palettePtr, paletteCount);
    ++stats_.encodes;
    stats_.encodeBytes += blob.size();
    return blob;
}

// ---------------------------------------------------------------------------
// Migrated methods: async chunk loading
// ---------------------------------------------------------------------------

bool WorldSession::dispatchAsyncLoad(int cx, int cy, int cz) {
    if (!store_ || !simSystem_)
        return false;

    if (!store_->hasChunk(cx, cy, cz)) {
        ++stats_.loadsDbMiss;
        return false;
    }
    ++stats_.loadsDispatched;

    auto& grid = simSystem_->simulationGrid();
    auto& registry = grid.registry();
    auto absent = simulation::addChunkRef(registry, cx, cy, cz);
    auto generating = simulation::transition<simulation::Absent, simulation::Generating>(absent, registry);

    int bufIdx = grid.currentWriteIndex();
    auto* buf = grid.writeBuffer(cx, cy, cz);
    if (!buf) {
        simulation::cancelAndRemove(generating, registry);
        simSystem_->activityTracker().remove(fabric::ChunkCoord{cx, cy, cz});
        return false;
    }

    auto* storePtr = store_.get();
    auto* worldGen = worldGen_;
    const auto* materials = simSystem_ ? &simSystem_->materials() : nullptr;

    auto future = scheduler_.submit([storePtr, cx, cy, cz, buf, worldGen, materials]() -> AsyncLoadResult {
        AsyncLoadResult r;
        try {
            auto blob = storePtr->loadChunk(cx, cy, cz);
            if (!blob) {
                FABRIC_LOG_WARN("asyncLoad({},{},{}): loadChunk returned nullopt", cx, cy, cz);
                return r;
            }

            bool isDelta = FchkCodec::isDelta(*blob);
            FchkDecoded decoded;
            if (isDelta && worldGen) {
                auto refBuf = std::make_unique<std::array<simulation::VoxelCell, simulation::K_CHUNK_VOLUME>>();
                worldGen->generateToBuffer(refBuf->data(), cx, cy, cz);
                if (materials) {
                    EssencePalette refPalette;
                    simulation::assignEssence(refBuf->data(), cx, cy, cz, *materials, refPalette, 42);
                }
                decoded = FchkCodec::decodeAny(*blob, refBuf->data());
            } else {
                decoded = FchkCodec::decodeAny(*blob);
            }

            const size_t expected = sizeof(*buf);
            if (decoded.cells.size() == expected) {
                std::memcpy(buf->data(), decoded.cells.data(), expected);
                r.paletteData = std::move(decoded.paletteData);
                r.paletteEntryCount = decoded.paletteEntryCount;
                r.success = true;
            } else {
                FABRIC_LOG_WARN("asyncLoad({},{},{}): cells size mismatch: got {} expected {}", cx, cy, cz,
                                decoded.cells.size(), expected);
            }
        } catch (const std::exception& ex) {
            FABRIC_LOG_WARN("asyncLoad({},{},{}): decode failed: {}", cx, cy, cz, ex.what());
        }
        return r;
    });

    PendingLoadMeta meta{bufIdx, generating};
    fabric::ChunkCoord coord{cx, cy, cz};
    if (!pendingLoads_.submit(coord, std::move(future), meta)) {
        simulation::cancelAndRemove(generating, registry);
        simSystem_->activityTracker().remove(coord);
        return false;
    }
    return true;
}

std::vector<ops::CompletedLoad> WorldSession::pollPendingLoads() {
    std::vector<ops::CompletedLoad> completions;
    auto entries = pendingLoads_.poll(maxLoadCompletions_);

    for (auto& entry : entries) {
        int cx = entry.key.x;
        int cy = entry.key.y;
        int cz = entry.key.z;
        auto& meta = entry.metadata;

        if (entry.cancelled) {
            ++stats_.loadsCancel;
            if (simSystem_) {
                auto& registry = simSystem_->simulationGrid().registry();
                simulation::cancelAndRemove(meta.generating, registry);
                simSystem_->activityTracker().remove(entry.key);
            }
            continue;
        }

        if (entry.result.success) {
            ++stats_.loadsOk;
            auto& grid = simSystem_->simulationGrid();
            auto& registry = grid.registry();
            simulation::ChunkBufferPolicyInputs bufferInputs;
            bufferInputs.sourceBufferIndex = meta.bufferIndex;
            bufferInputs.materials = &simSystem_->materials();
            bufferInputs.paletteData =
                std::span<const float>(entry.result.paletteData.data(), entry.result.paletteData.size());
            simulation::finalizeChunkBuffers(grid, cx, cy, cz,
                                             simulation::chunkBufferFinalizationOptionsForCause(
                                                 simulation::ChunkFinalizationCause::AsyncLoadReady, bufferInputs));
            simulation::transition<simulation::Generating, simulation::Active>(meta.generating, registry);
            simulation::finalizeChunkActivation(
                simSystem_->activityTracker(), grid, cx, cy, cz,
                simulation::chunkActivationOptionsForCause(simulation::ChunkFinalizationCause::AsyncLoadReady));

            // F42: Snapshot the chunk's initial state so replay has an anchor.
            if (snapshotScheduler_)
                snapshotScheduler_->markDirty(cx, cy, cz);

            completions.push_back({cx, cy, cz, meta.bufferIndex, true});
        } else {
            ++stats_.loadsFail;
            FABRIC_LOG_WARN("pollPendingLoads({},{},{}): load failed", cx, cy, cz);
            if (simSystem_) {
                auto& registry = simSystem_->simulationGrid().registry();
                simulation::cancelAndRemove(meta.generating, registry);
                simSystem_->activityTracker().remove(entry.key);
            }
            completions.push_back({cx, cy, cz, meta.bufferIndex, false});
        }
    }

    return completions;
}

bool WorldSession::cancelPendingLoad(int cx, int cy, int cz) {
    return pendingLoads_.cancel(fabric::ChunkCoord{cx, cy, cz});
}

bool WorldSession::hasPendingLoad(int cx, int cy, int cz) const {
    return pendingLoads_.has(fabric::ChunkCoord{cx, cy, cz});
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

    int unloadRadius = lodRadius + lodHysteresis_;

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

void WorldSession::enqueueResidentChunksForShutdown() {
    if (!simSystem_ || !saveService_)
        return;

    auto& grid = simSystem_->simulationGrid();
    auto& registry = grid.registry();
    for (const auto& [cx, cy, cz] : grid.allChunks()) {
        const auto* slot = registry.find(cx, cy, cz);
        if (!slot || slot->state != simulation::ChunkSlotState::Active || !slot->isMaterialized())
            continue;

        auto blob = encodeChunkBlob(cx, cy, cz);
        if (!blob.empty())
            saveService_->enqueuePrepared(cx, cy, cz, std::move(blob));
    }
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

ChunkSnapshotProvider* WorldSession::chunkSnapshotProvider() const {
    return chunkSnapshotProvider_.get();
}

persistence::ReplayExecutor* WorldSession::replayExecutor() const {
    return replayExecutor_.get();
}

WorldSession::RuntimeStatusSnapshot WorldSession::runtimeStatusSnapshot() const {
    RuntimeStatusSnapshot snapshot;
    snapshot.pendingLoads = pendingLoads_.size();
    if (saveService_)
        snapshot.saveActivity = saveService_->activitySnapshot();
    return snapshot;
}

// ---------------------------------------------------------------------------
// Async mutation submit (Phase III: ops-as-values)
// ---------------------------------------------------------------------------

bool WorldSession::submit(ops::LoadChunk op) {
    return dispatchAsyncLoad(op.cx, op.cy, op.cz);
}

void WorldSession::submit(ops::SaveChunk op) {
    if (saveService_)
        saveService_->markDirty(op.cx, op.cy, op.cz);
}

void WorldSession::submit(ops::PersistChunk op) {
    auto blob = encodeChunkBlob(op.cx, op.cy, op.cz);
    if (!blob.empty() && saveService_)
        saveService_->enqueuePrepared(op.cx, op.cy, op.cz, std::move(blob));
}

void WorldSession::submit(ops::RemoveChunk op) {
    fabric::ChunkCoord coord{op.cx, op.cy, op.cz};
    auto it = chunkEntities_.find(coord);
    if (it != chunkEntities_.end()) {
        it->second.destruct();
        chunkEntities_.erase(it);
    }
    lodChunks_.erase(coord);
    if (simSystem_)
        simSystem_->removeChunk(op.cx, op.cy, op.cz);
}

bool WorldSession::submit(ops::CancelPendingLoad op) {
    return cancelPendingLoad(op.cx, op.cy, op.cz);
}

void WorldSession::submit(ops::GenerateChunks op) {
    if (simSystem_)
        simSystem_->generateChunksBatch(op.coords);
}

void WorldSession::submit(ops::Tick op) {
    flushPendingChanges();
    updateSaveService(op.dt);
    updateSnapshotScheduler(op.dt);
    updatePruningScheduler(op.dt);

    checkpointElapsed_ += op.dt;
    if (checkpointElapsed_ >= K_CHECKPOINT_INTERVAL_SECONDS) {
        checkpointElapsed_ = 0.0f;
        writerQueue_.submit([this]() { store_->maybeCheckpoint(); });
    }

    if (++stats_.ticks >= K_STATS_INTERVAL) {
        auto status = runtimeStatusSnapshot();
        bool any = stats_.encodes > 0 || stats_.loadsDispatched > 0 || stats_.loadsDbMiss > 0 || stats_.loadsOk > 0 ||
                   stats_.loadsFail > 0 || status.saveActivity.dirtyChunks > 0 ||
                   status.saveActivity.savingChunks > 0 || status.saveActivity.preparedChunks > 0 ||
                   status.saveActivity.hasError;
        if (any) {
            const char* errorText = status.saveActivity.hasError ? status.saveActivity.lastError.c_str() : "none";
            FABRIC_LOG_INFO("persistence: dir='{}' encodes={} ({}B, {}zeroDiff) loads={} ({}ok {}fail {}miss {}cancel) "
                            "pendingLoads={} save(dirty={} saving={} prepared={} lastOk={} error={})",
                            worldDir_, stats_.encodes, stats_.encodeBytes, stats_.zeroDiffEncodes,
                            stats_.loadsDispatched, stats_.loadsOk, stats_.loadsFail, stats_.loadsDbMiss,
                            stats_.loadsCancel, status.pendingLoads, status.saveActivity.dirtyChunks,
                            status.saveActivity.savingChunks, status.saveActivity.preparedChunks,
                            status.saveActivity.lastSuccessfulSerial, errorText);
        }
        stats_ = {};
    }
}

void WorldSession::submit(ops::UpdateLODRing op) {
    int cx = static_cast<int>(std::floor(op.px / recurse::simulation::K_CHUNK_SIZE));
    int cy = static_cast<int>(std::floor(op.py / recurse::simulation::K_CHUNK_SIZE));
    int cz = static_cast<int>(std::floor(op.pz / recurse::simulation::K_CHUNK_SIZE));
    updateLODRing(cx, cy, cz, op.chunkRadius, op.lodRadius, op.genBudget);
}

} // namespace recurse
