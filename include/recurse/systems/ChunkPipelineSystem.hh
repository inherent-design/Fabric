#pragma once

#include "fabric/core/SystemBase.hh"
#include "recurse/world/ChunkStreaming.hh"
#include <flecs.h>
#include <memory>
#include <unordered_map>

namespace recurse::systems {

class TerrainSystem;
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

  private:
    TerrainSystem* terrain_ = nullptr;
    VoxelSimulationSystem* simSystem_ = nullptr;
    PhysicsGameSystem* physics_ = nullptr;
    CharacterMovementSystem* charMovement_ = nullptr;

    std::unique_ptr<ChunkStreamingManager> streaming_;
    std::unordered_map<ChunkCoord, flecs::entity, ChunkCoordHash> chunkEntities_;
};

} // namespace recurse::systems
