#pragma once

#include "fabric/core/SystemBase.hh"
#include "recurse/physics/PhysicsWorld.hh"
#include "recurse/physics/Ragdoll.hh"

namespace recurse::systems {

class TerrainSystem;

/// Owns the Jolt PhysicsWorld and Ragdoll subsystem.
/// Steps physics at fixed rate and rebuilds chunk collision
/// when voxel data changes.
class PhysicsGameSystem : public fabric::System<PhysicsGameSystem> {
  public:
    PhysicsGameSystem() = default;

    void init(fabric::AppContext& ctx) override;
    void shutdown() override;
    void fixedUpdate(fabric::AppContext& ctx, float fixedDt) override;

    void configureDependencies() override;

    PhysicsWorld& physicsWorld() { return physicsWorld_; }
    const PhysicsWorld& physicsWorld() const { return physicsWorld_; }
    JPH::PhysicsSystem* joltSystem() { return physicsWorld_.joltSystem(); }
    Ragdoll& ragdoll() { return ragdoll_; }

    /// Clear all terrain collision bodies (for world reset)
    void clearAllCollisions();

  private:
    TerrainSystem* terrain_ = nullptr;
    PhysicsWorld physicsWorld_;
    Ragdoll ragdoll_;
};

} // namespace recurse::systems
