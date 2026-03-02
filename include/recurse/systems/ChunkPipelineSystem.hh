#pragma once

#include "fabric/core/SystemBase.hh"
#include "recurse/world/ChunkStreaming.hh"
#include "recurse/world/VoxelMesher.hh"
#include <flecs.h>
#include <memory>
#include <unordered_map>
#include <unordered_set>

namespace recurse {
class ChunkMeshManager;
} // namespace recurse

namespace recurse::systems {

class TerrainSystem;
class PhysicsGameSystem;
class CharacterMovementSystem;

/// Owns chunk streaming, CPU meshing, and GPU upload pipeline.
/// Handles chunk load/unload lifecycle including ECS entity creation,
/// terrain generation delegation, and bgfx buffer management.
class ChunkPipelineSystem : public fabric::System<ChunkPipelineSystem> {
  public:
    ChunkPipelineSystem() = default;
    ~ChunkPipelineSystem() override;

    void init(fabric::AppContext& ctx) override;
    void shutdown() override;
    void fixedUpdate(fabric::AppContext& ctx, float fixedDt) override;

    void configureDependencies() override;

    // Accessors
    const std::unordered_map<ChunkCoord, ChunkMesh, ChunkCoordHash>& gpuMeshes() const { return gpuMeshes_; }
    const std::unordered_map<ChunkCoord, flecs::entity, ChunkCoordHash>& chunkEntities() const {
        return chunkEntities_;
    }
    ChunkStreamingManager& streaming() { return *streaming_; }
    ChunkMeshManager& meshManager() { return *meshManager_; }
    size_t dirtyCount() const;

  private:
    ChunkMesh uploadChunkMesh(const ChunkMeshData& data);

    TerrainSystem* terrain_ = nullptr;
    PhysicsGameSystem* physics_ = nullptr;
    CharacterMovementSystem* charMovement_ = nullptr;

    std::unique_ptr<ChunkStreamingManager> streaming_;
    std::unique_ptr<ChunkMeshManager> meshManager_;
    std::unordered_map<ChunkCoord, ChunkMesh, ChunkCoordHash> gpuMeshes_;
    std::unordered_set<ChunkCoord, ChunkCoordHash> gpuUploadQueue_;
    std::unordered_map<ChunkCoord, flecs::entity, ChunkCoordHash> chunkEntities_;
};

} // namespace recurse::systems
