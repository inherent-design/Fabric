#pragma once

#include "fabric/ecs/ECS.hh"

#include <cstddef>
#include <vector>

namespace fabric {

class SystemBase;
class SystemRegistry;

/// Opt-in mixin for systems that hold per-world mutable state.
/// Systems inherit WorldAware alongside System<T> to receive
/// lifecycle callbacks on world transitions. The coordinator
/// discovers participants via dynamic_cast during initialization.
class WorldAware {
  public:
    virtual ~WorldAware() = default;

    /// Called after a new world is loaded and ready for simulation.
    /// Systems should initialize per-world state here.
    virtual void onWorldBegin() = 0;

    /// Called before the current world is torn down.
    /// Systems must release all per-world state: clear collections,
    /// unsubscribe listeners, free GPU resources, reset counters.
    /// Called in reverse init order (dependencies torn down first).
    virtual void onWorldEnd() = 0;
};

/// Coordinates world lifecycle transitions across WorldAware systems.
/// Discovers participants via dynamic_cast on SystemRegistry init order.
/// Dispatches onWorldBegin in init order, onWorldEnd in reverse.
class WorldLifecycleCoordinator {
  public:
    /// Discover WorldAware systems from the registry.
    /// Call after SystemRegistry::initAll() completes.
    void discover(SystemRegistry& registry, World& world);

    /// Notify all participants that a new world has begun.
    /// Called in SystemRegistry init order.
    void beginWorld();

    /// Notify all participants that the current world is ending.
    /// Called in reverse SystemRegistry init order.
    void endWorld();

    /// Number of discovered WorldAware participants.
    size_t participantCount() const;

  private:
    std::vector<WorldAware*> participants_;
    World* world_ = nullptr;
};

} // namespace fabric
