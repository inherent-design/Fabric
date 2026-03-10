#pragma once

#include "fabric/core/SystemBase.hh"
#include "recurse/character/GameConstants.hh"
#include "recurse/world/ChunkStreaming.hh"
#include <flecs.h>
#include <future>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace recurse {
class ChunkStore;
class ChunkSaveService;
} // namespace recurse

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

    // Optional persistence (null if no world loaded)
    recurse::ChunkStore* chunkStore_ = nullptr;
    recurse::ChunkSaveService* saveService_ = nullptr;

    // LOD ring: chunks outside full-res radius, inside lod_radius
    std::unordered_set<ChunkCoord, ChunkCoordHash> lodChunks_;
    int lodRadius_ = 0;
    int lodGenBudget_ = 4;

    void updateLODRing(int centerCX, int centerCY, int centerCZ);

    // Async chunk loading
    struct PendingChunkLoad {
        std::future<bool> result;
        int cx, cy, cz;
        bool cancelled = false;
    };
    std::vector<PendingChunkLoad> pendingLoads_;

    bool dispatchAsyncLoad(int cx, int cy, int cz);
    void pollPendingLoads(fabric::AppContext& ctx);
    bool cancelPendingLoad(int cx, int cy, int cz);
    bool hasPendingLoad(int cx, int cy, int cz) const;

    void saveChunkToDisk(int cx, int cy, int cz);
};

} // namespace recurse::systems
