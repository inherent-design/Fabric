#pragma once

#include "fabric/core/SystemBase.hh"
#include "recurse/config/RecurseConfig.hh"
#include "recurse/physics/PhysicsWorld.hh"
#include "recurse/physics/Ragdoll.hh"
#include "recurse/world/ChunkStreaming.hh"

#include <climits>
#include <string>
#include <unordered_set>
#include <vector>

namespace fabric {
class EventDispatcher;
class JobScheduler;
} // namespace fabric

namespace recurse::systems {

class TerrainSystem;
class VoxelSimulationSystem;

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

    void onWorldBegin();
    void onWorldEnd();

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

    void setFocalPoints(const std::vector<recurse::FocalPoint>& points);

    const std::unordered_set<recurse::ChunkCoord, recurse::ChunkCoordHash>& dirtyChunks() const {
        return dirtyCollisionChunks_;
    }

    void insertDirtyChunk(int cx, int cy, int cz) { dirtyCollisionChunks_.insert({cx, cy, cz}); }

    void setVoxelSimForTesting(VoxelSimulationSystem* sim) { voxelSim_ = sim; }

  private:
    TerrainSystem* terrain_ = nullptr;
    VoxelSimulationSystem* voxelSim_ = nullptr;
    fabric::JobScheduler* scheduler_ = nullptr;
    fabric::EventDispatcher* dispatcher_ = nullptr;
    PhysicsWorld physicsWorld_;
    Ragdoll ragdoll_;
    std::unordered_set<recurse::ChunkCoord, recurse::ChunkCoordHash> dirtyCollisionChunks_;
    std::vector<recurse::FocalPoint> focalPoints_;
    std::vector<recurse::CollisionCenter> lastFocalChunkCoords_;
    std::string voxelChangedListenerId_;
    bool worldActive_ = false;

    int collisionBudget_ = RecurseConfig::K_DEFAULT_COLLISION_BUDGET;
    int collisionRadius_ = RecurseConfig::K_DEFAULT_COLLISION_RADIUS;
};

} // namespace recurse::systems
