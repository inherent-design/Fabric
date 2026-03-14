#pragma once

#include "fabric/core/SystemBase.hh"
#include "fabric/ecs/ECS.hh"
#include "recurse/character/GameConstants.hh"
#include "recurse/world/ChunkStreaming.hh"
#include <chrono>
#include <memory>
#include <string>
#include <unordered_map>

namespace fabric {
class EventDispatcher;
class JobScheduler;
} // namespace fabric

namespace recurse {
class WorldSession;
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
/// Load: streaming detects new chunk -> VoxelSimulationSystem::generateChunk() fills grid
///   -> dirty flags set -> VoxelSimulationSystem picks up -> VoxelMeshingSystem meshes.
/// Unload: streaming detects far chunk -> VoxelSimulationSystem::removeChunk() -> ECS destroy.
class ChunkPipelineSystem : public fabric::System<ChunkPipelineSystem> {
  public:
    ChunkPipelineSystem();
    ~ChunkPipelineSystem() override;

    void doInit(fabric::AppContext& ctx) override;
    void doShutdown() override;
    void fixedUpdate(fabric::AppContext& ctx, float fixedDt) override;

    void configureDependencies() override;

    // Accessors
    const std::unordered_map<ChunkCoord, flecs::entity, ChunkCoordHash>& chunkEntities() const;
    ChunkStreamingManager& streaming() { return *streaming_; }
    ChunkPipelineDebugInfo debugInfo() const;

    /// Wire persistence for a world directory. Creates a WorldSession with owned store and services.
    void loadWorld(const std::string& worldDir, fabric::JobScheduler& scheduler);

    /// Tear down persistence. Safe to call when no world is loaded.
    void unloadWorld();

  private:
    LODSystem* lodSystem_ = nullptr;
    TerrainSystem* terrain_ = nullptr;
    VoxelMeshingSystem* meshingSystem_ = nullptr;
    VoxelSimulationSystem* simSystem_ = nullptr;
    PhysicsGameSystem* physics_ = nullptr;
    CharacterMovementSystem* charMovement_ = nullptr;

    std::unique_ptr<ChunkStreamingManager> streaming_;
    std::unique_ptr<recurse::WorldSession> session_;

    fabric::EventDispatcher* dispatcher_ = nullptr;
    flecs::world* ecsWorld_ = nullptr;

    float lastPlayerX_ = K_DEFAULT_SPAWN_X;
    float lastPlayerY_ = K_DEFAULT_SPAWN_Y;
    float lastPlayerZ_ = K_DEFAULT_SPAWN_Z;

    int loadsThisFrame_ = 0;
    int unloadsThisFrame_ = 0;

    std::chrono::steady_clock::time_point lastFrameTime_{};
    float frameTimeEma_{16.0f};

    int lodRadius_ = 0;
    int lodGenBudget_ = 4;
};

} // namespace recurse::systems
