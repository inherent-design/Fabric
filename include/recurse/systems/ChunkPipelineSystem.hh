#pragma once

#include "fabric/core/ECS.hh"
#include "fabric/core/SystemBase.hh"
#include "recurse/character/GameConstants.hh"
#include "recurse/components/StreamSource.hh"
#include "recurse/persistence/ChunkSaveService.hh"
#include "recurse/persistence/ChunkStore.hh"
#include "recurse/persistence/PruningScheduler.hh"
#include "recurse/persistence/SnapshotScheduler.hh"
#include "recurse/persistence/WorldTransactionStore.hh"
#include "recurse/world/ChunkStreaming.hh"
#include <climits>
#include <flecs.h>
#include <future>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace fabric {
class JobScheduler;
}

namespace recurse::systems {

struct ChunkPipelineDebugInfo {
    int trackedChunks = 0;
    int chunksLoadedThisFrame = 0;
    int chunksUnloadedThisFrame = 0;
    float currentStreamingRadius = 0.0f;
};

class LODSystem;
class TerrainSystem;
class VoxelMeshingSystem;
class VoxelSimulationSystem;
class PhysicsGameSystem;
class CharacterMovementSystem;

/// Owns chunk streaming and ECS entity lifecycle.
/// Load: streaming detects new chunk → VoxelSimulationSystem::generateChunk() fills grid
///   → dirty flags set → VoxelSimulationSystem picks up → VoxelMeshingSystem meshes.
/// Unload: streaming detects far chunk → VoxelSimulationSystem::removeChunk() → ECS destroy.
class ChunkPipelineSystem : public fabric::System<ChunkPipelineSystem> {
  public:
    ChunkPipelineSystem() = default;
    ~ChunkPipelineSystem() override;

    void doInit(fabric::AppContext& ctx) override;
    void doShutdown() override;
    void fixedUpdate(fabric::AppContext& ctx, float fixedDt) override;

    void configureDependencies() override;

    // Accessors
    const std::unordered_map<ChunkCoord, flecs::entity, ChunkCoordHash>& chunkEntities() const {
        return chunkEntities_;
    }
    ChunkStreamingManager& streaming() { return *streaming_; }
    ChunkPipelineDebugInfo debugInfo() const;

    /// Set optional persistence layer. Null = no persistence (generate every time).
    void setChunkStore(recurse::ChunkStore* store) { chunkStore_ = store; }
    void setChunkSaveService(recurse::ChunkSaveService* svc) { saveService_ = svc; }

    /// Wire persistence for a world directory. Creates owned ChunkStore + ChunkSaveService.
    void loadWorld(const std::string& worldDir, fabric::JobScheduler& scheduler);

    /// Flush pending saves and tear down persistence. Safe to call when no world is loaded.
    void unloadWorld();

  private:
    LODSystem* lodSystem_ = nullptr;
    TerrainSystem* terrain_ = nullptr;
    VoxelMeshingSystem* meshingSystem_ = nullptr;
    VoxelSimulationSystem* simSystem_ = nullptr;
    PhysicsGameSystem* physics_ = nullptr;
    CharacterMovementSystem* charMovement_ = nullptr;

    std::unique_ptr<ChunkStreamingManager> streaming_;
    std::unordered_map<ChunkCoord, flecs::entity, ChunkCoordHash> chunkEntities_;

    float lastPlayerX_ = K_DEFAULT_SPAWN_X;
    float lastPlayerY_ = K_DEFAULT_SPAWN_Y;
    float lastPlayerZ_ = K_DEFAULT_SPAWN_Z;

    int loadsThisFrame_ = 0;
    int unloadsThisFrame_ = 0;

    // Persistence: owned storage created by loadWorld(), torn down by unloadWorld().
    // Raw pointers alias the owned ptrs (or can be set externally via setChunkStore/setChunkSaveService).
    std::unique_ptr<recurse::ChunkStore> ownedStore_;
    std::unique_ptr<recurse::ChunkSaveService> ownedSaveService_;
    std::unique_ptr<recurse::WorldTransactionStore> ownedTransactionStore_;
    std::unique_ptr<recurse::SnapshotScheduler> ownedSnapshotScheduler_;
    std::unique_ptr<recurse::PruningScheduler> ownedPruningScheduler_;
    recurse::ChunkStore* chunkStore_ = nullptr;
    recurse::ChunkSaveService* saveService_ = nullptr;
    recurse::WorldTransactionStore* transactionStore_ = nullptr;

    // LOD ring: chunks outside full-res radius, inside lod_radius
    std::unordered_set<ChunkCoord, ChunkCoordHash> lodChunks_;
    int lodRadius_ = 0;
    int lodGenBudget_ = 4;
    int lastLodCX_ = INT_MIN;
    int lastLodCY_ = INT_MIN;
    int lastLodCZ_ = INT_MIN;

    std::optional<flecs::query<const fabric::Position, const recurse::StreamSource>> streamSourceQuery_;

    void updateLODRing(int centerCX, int centerCY, int centerCZ);

    // Async chunk loading
    struct AsyncLoadResult {
        bool success = false;
        std::vector<float> paletteData;
        uint16_t paletteEntryCount = 0;
    };
    struct PendingChunkLoad {
        std::future<AsyncLoadResult> result;
        int cx, cy, cz;
        bool cancelled = false;
    };
    std::vector<PendingChunkLoad> pendingLoads_;

    bool dispatchAsyncLoad(int cx, int cy, int cz);
    void pollPendingLoads(fabric::AppContext& ctx);
    bool cancelPendingLoad(int cx, int cy, int cz);
    bool hasPendingLoad(int cx, int cy, int cz) const;

    void saveChunkToDisk(int cx, int cy, int cz);
    ChunkBlob encodeChunkBlob(int cx, int cy, int cz);
};

} // namespace recurse::systems
