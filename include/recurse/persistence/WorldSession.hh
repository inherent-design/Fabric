#pragma once

#include "fabric/core/CompilerHints.hh"
#include "fabric/fx/Error.hh"
#include "fabric/fx/Result.hh"
#include "fabric/platform/ScopedTaskGroup.hh"
#include "fabric/platform/WriterQueue.hh"
#include "fabric/world/ChunkCoord.hh"
#include "recurse/persistence/ChunkStore.hh"
#include "recurse/persistence/SqliteChunkStore.hh"
#include "recurse/persistence/WorldTransactionStore.hh"
#include "recurse/simulation/ChunkState.hh"
#include "recurse/simulation/SimulationGrid.hh"
#include "recurse/systems/VoxelSimulationSystem.hh"
#include "recurse/world/ChunkOps.hh"
#include <climits>
#include <future>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <flecs.h>

namespace fabric {
class EventDispatcher;
class JobScheduler;
struct Position;
} // namespace fabric

namespace recurse {

class SqliteChunkStore;
class SqliteTransactionStore;
class ChunkSaveService;
class SnapshotScheduler;
class PruningScheduler;
struct StreamSource;

namespace systems {
class VoxelSimulationSystem;
class VoxelMeshingSystem;
class LODSystem;
class PhysicsGameSystem;
class TerrainSystem;
} // namespace systems

/// RAII session owning all per-world persistence and streaming state.
/// Created via static factory open(); destroyed on world unload.
/// First production consumer of fabric::fx::Result.
class WorldSession {
  public:
    WorldSession(const WorldSession&) = delete;
    WorldSession& operator=(const WorldSession&) = delete;

    ~WorldSession();

    /// Open a world session. Returns IOError if the database cannot be opened.
    static fabric::fx::Result<std::unique_ptr<WorldSession>, fabric::fx::IOError>
    open(const std::string& worldDir, fabric::EventDispatcher& dispatcher, fabric::JobScheduler& scheduler,
         flecs::world& ecsWorld, systems::VoxelSimulationSystem* simSystem, systems::VoxelMeshingSystem* meshingSystem,
         systems::LODSystem* lodSystem, systems::PhysicsGameSystem* physicsSystem,
         systems::TerrainSystem* terrainSystem);

    // --- Inner types (migrated from ChunkPipelineSystem) ---

    struct AsyncLoadResult {
        bool success = false;
        std::vector<float> paletteData;
        uint16_t paletteEntryCount = 0;
    };

    struct PendingLoadMeta {
        int bufferIndex = 0;
        simulation::ChunkRef<simulation::Generating> generating;
    };

    // --- Methods migrated from ChunkPipelineSystem ---

    ChunkBlob encodeChunkBlob(int cx, int cy, int cz);
    bool dispatchAsyncLoad(int cx, int cy, int cz);
    std::vector<ops::CompletedLoad> pollPendingLoads();
    bool cancelPendingLoad(int cx, int cy, int cz);
    bool hasPendingLoad(int cx, int cy, int cz) const;
    void updateLODRing(int playerCx, int playerCy, int playerCz, int streamingRadius, int lodRadius, int lodGenBudget);

    // --- Session lifecycle methods ---

    void bufferVoxelChange(const VoxelChange& change);
    void flushPendingChanges();
    void updateSaveService(float dt);
    void updateSnapshotScheduler(float dt);
    void updatePruningScheduler(float dt);

    // --- Accessors ---

    SqliteChunkStore* chunkStore() const;
    ChunkSaveService* saveService() const;
    WorldTransactionStore* transactionStore() const;

    auto& chunkEntities() { return chunkEntities_; }
    const auto& chunkEntities() const { return chunkEntities_; }
    auto& pendingLoads() { return pendingLoads_; }
    const auto& pendingLoads() const { return pendingLoads_; }
    auto& lodChunks() { return lodChunks_; }
    const auto& lodChunks() const { return lodChunks_; }

    int lastLodCX() const { return lastLodCX_; }
    int lastLodCY() const { return lastLodCY_; }
    int lastLodCZ() const { return lastLodCZ_; }

    void setMaxLoadCompletions(int n) { maxLoadCompletions_ = n; }
    void setLodHysteresis(int n) { lodHysteresis_ = n; }

    std::optional<flecs::query<const fabric::Position, const StreamSource>>& streamSourceQuery() {
        return streamSourceQuery_;
    }

    // --- Sync read resolve (Phase III: ops-as-values) ---

    FABRIC_ALWAYS_INLINE bool resolve(const ops::HasChunk& op) {
        return simSystem_->simulationGrid().hasChunk(op.cx, op.cy, op.cz);
    }

    FABRIC_ALWAYS_INLINE const simulation::ChunkSlot* resolve(const ops::FindSlot& op) {
        return simSystem_->simulationGrid().registry().find(op.cx, op.cy, op.cz);
    }

    FABRIC_ALWAYS_INLINE bool resolve(const ops::IsInSavedRegion& op) {
        return store_->isInSavedRegion(op.cx, op.cy, op.cz);
    }

    FABRIC_ALWAYS_INLINE bool resolve(const ops::HasPendingLoad& op) { return hasPendingLoad(op.cx, op.cy, op.cz); }

    FABRIC_ALWAYS_INLINE bool resolve(const ops::QueryChunkEntities& op) { return chunkEntities_.contains(op.coord); }

    FABRIC_ALWAYS_INLINE const simulation::VoxelCell* resolve(const ops::ReadBuffer& op) {
        auto* arr = simSystem_->simulationGrid().readBuffer(op.cx, op.cy, op.cz);
        return arr ? arr->data() : nullptr;
    }

    FABRIC_ALWAYS_INLINE simulation::VoxelCell* resolve(const ops::WriteBuffer& op) {
        auto* arr = simSystem_->simulationGrid().writeBuffer(op.cx, op.cy, op.cz);
        return arr ? arr->data() : nullptr;
    }

    FABRIC_ALWAYS_INLINE int resolve(const ops::ChunkCount&) {
        return static_cast<int>(simSystem_->simulationGrid().registry().chunkCount());
    }

    FABRIC_ALWAYS_INLINE int resolve(const ops::ActiveChunkCount&) {
        return static_cast<int>(simSystem_->activeChunkCount());
    }

    FABRIC_ALWAYS_INLINE std::vector<ops::CompletedLoad> resolve(const ops::PollPendingLoads& op) {
        setMaxLoadCompletions(op.maxCompletions);
        return pollPendingLoads();
    }

    FABRIC_ALWAYS_INLINE int resolve(const ops::QueryLODChunks&) { return static_cast<int>(lodChunks_.size()); }

    // --- Async mutation submit (Phase III: ops-as-values) ---

    bool submit(ops::LoadChunk op);
    void submit(ops::SaveChunk op);
    void submit(ops::PersistChunk op);
    void submit(ops::RemoveChunk op);
    bool submit(ops::CancelPendingLoad op);
    void submit(ops::GenerateChunks op);
    void submit(ops::Tick op);
    void submit(ops::UpdateLODRing op);

  private:
    WorldSession(const std::string& worldDir, fabric::EventDispatcher& dispatcher, fabric::JobScheduler& scheduler,
                 flecs::world& ecsWorld, std::unique_ptr<SqliteChunkStore> store,
                 systems::VoxelSimulationSystem* simSystem, systems::VoxelMeshingSystem* meshingSystem,
                 systems::LODSystem* lodSystem, systems::PhysicsGameSystem* physicsSystem,
                 systems::TerrainSystem* terrainSystem);

    // Owned resources (declaration order = reverse destruction order)
    fabric::platform::WriterQueue writerQueue_;
    std::unique_ptr<SqliteChunkStore> store_;
    std::unique_ptr<SqliteTransactionStore> txStore_;
    std::unique_ptr<ChunkSaveService> saveService_;
    std::unique_ptr<SnapshotScheduler> snapshotScheduler_;
    std::unique_ptr<PruningScheduler> pruningScheduler_;

    // Per-world state
    std::unordered_map<fabric::ChunkCoord, flecs::entity, fabric::ChunkCoordHash> chunkEntities_;
    fabric::platform::ScopedTaskGroup<fabric::ChunkCoord, AsyncLoadResult, fabric::ChunkCoordHash, PendingLoadMeta>
        pendingLoads_;
    std::unordered_set<fabric::ChunkCoord, fabric::ChunkCoordHash> lodChunks_;
    int lastLodCX_ = INT_MIN;
    int lastLodCY_ = INT_MIN;
    int lastLodCZ_ = INT_MIN;
    std::vector<VoxelChange> pendingChanges_;
    std::optional<flecs::query<const fabric::Position, const StreamSource>> streamSourceQuery_;

    int maxLoadCompletions_ = 16;
    int lodHysteresis_ = 2;
    float checkpointElapsed_{0.0f};

    // Non-owning references (outlive session)
    fabric::EventDispatcher& dispatcher_;
    fabric::JobScheduler& scheduler_;
    flecs::world& ecsWorld_;
    std::string listenerHandlerId_;

    systems::VoxelSimulationSystem* simSystem_ = nullptr;
    systems::VoxelMeshingSystem* meshingSystem_ = nullptr;
    systems::LODSystem* lodSystem_ = nullptr;
    systems::PhysicsGameSystem* physicsSystem_ = nullptr;
    systems::TerrainSystem* terrainSystem_ = nullptr;
};

} // namespace recurse
