#pragma once

#include "fabric/core/SystemBase.hh"
#include "recurse/physics/PhysicsWorld.hh"
#include "recurse/physics/Ragdoll.hh"

#include <unordered_set>

namespace fabric {
class JobScheduler;
} // namespace fabric

namespace recurse::systems {

class TerrainSystem;
class VoxelSimulationSystem;

// Per-frame cap; matches scale of meshBudget_ and genBudget_
inline constexpr int K_COLLISION_BUDGET_PER_FRAME = 8;

/// Owns the Jolt PhysicsWorld and Ragdoll subsystem.
/// Steps physics at fixed rate and rebuilds chunk collision
/// when voxel data changes.
class PhysicsGameSystem : public fabric::System<PhysicsGameSystem> {
  public:
    PhysicsGameSystem() = default;

    void doInit(fabric::AppContext& ctx) override;
    void doShutdown() override;
    void fixedUpdate(fabric::AppContext& ctx, float fixedDt) override;

    void configureDependencies() override;

    PhysicsWorld& physicsWorld() { return physicsWorld_; }
    const PhysicsWorld& physicsWorld() const { return physicsWorld_; }
    JPH::PhysicsSystem* joltSystem() { return physicsWorld_.joltSystem(); }
    Ragdoll& ragdoll() { return ragdoll_; }

    /// Clear all terrain collision bodies (for world reset)
    void clearAllCollisions();

    /// Remove a chunk from the pending collision rebuild set.
    /// Called by ChunkPipelineSystem before removeChunkCollision() during unload
    /// to prevent rebuilding collision for a chunk whose simulation data is gone.
    void removeDirtyChunk(int cx, int cy, int cz);

    void setPlayerPosition(float x, float y, float z);

    const std::unordered_set<recurse::ChunkKey, recurse::ChunkKeyHash>& dirtyChunks() const {
        return dirtyCollisionChunks_;
    }

    void insertDirtyChunk(int cx, int cy, int cz) { dirtyCollisionChunks_.insert({cx, cy, cz}); }

    void setVoxelSimForTesting(VoxelSimulationSystem* sim) { voxelSim_ = sim; }

  private:
    TerrainSystem* terrain_ = nullptr;
    VoxelSimulationSystem* voxelSim_ = nullptr;
    fabric::JobScheduler* scheduler_ = nullptr;
    PhysicsWorld physicsWorld_;
    Ragdoll ragdoll_;
    std::unordered_set<recurse::ChunkKey, recurse::ChunkKeyHash> dirtyCollisionChunks_;
    float playerX_ = 0.0f, playerY_ = 0.0f, playerZ_ = 0.0f;
};

} // namespace recurse::systems
