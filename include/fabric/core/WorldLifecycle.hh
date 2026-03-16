#pragma once

#include "fabric/ecs/ECS.hh"

#include <cstddef>
#include <functional>
#include <vector>

namespace fabric {

class SystemRegistry;

/// Concept: system provides onWorldBegin callback.
template <typename T>
concept HasWorldBegin = requires(T& t) {
    { t.onWorldBegin() } -> std::same_as<void>;
};

/// Concept: system provides onWorldEnd callback.
template <typename T>
concept HasWorldEnd = requires(T& t) {
    { t.onWorldEnd() } -> std::same_as<void>;
};

/// Coordinates world lifecycle transitions across participating systems.
/// Systems register explicitly during doInit via registerParticipant.
/// No virtual inheritance required; systems define onWorldBegin/onWorldEnd
/// as regular methods and register callbacks.
class WorldLifecycleCoordinator {
  public:
    struct Participant {
        std::function<void()> onBegin;
        std::function<void()> onEnd;
    };

    /// Register a system as a world lifecycle participant.
    /// Call from SystemBase::doInit(). Order of registration determines
    /// dispatch order (beginWorld in registration order, endWorld in reverse).
    void registerParticipant(std::function<void()> onBegin, std::function<void()> onEnd);

    /// Store the ECS world reference for WorldScoped entity cleanup.
    /// Call after SystemRegistry::initAll().
    void setWorld(World& world);

    /// Notify all participants that a new world has begun.
    /// Called in registration order.
    void beginWorld();

    /// Notify all participants that the current world is ending.
    /// Called in reverse registration order.
    void endWorld();

    /// Number of registered participants.
    size_t participantCount() const;

  private:
    std::vector<Participant> participants_;
    World* world_ = nullptr;
};

} // namespace fabric
