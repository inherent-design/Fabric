#include "fabric/core/WorldLifecycle.hh"
#include "fabric/ecs/WorldScoped.hh"
#include "fabric/log/Log.hh"

namespace fabric {

void WorldLifecycleCoordinator::registerParticipant(std::function<void()> onBegin, std::function<void()> onEnd) {
    participants_.push_back({std::move(onBegin), std::move(onEnd)});
}

void WorldLifecycleCoordinator::setWorld(World& world) {
    world_ = &world;
    FABRIC_LOG_INFO("WorldLifecycleCoordinator: {} participants registered", participants_.size());
}

void WorldLifecycleCoordinator::beginWorld() {
    FABRIC_LOG_INFO("WorldLifecycleCoordinator: beginWorld ({} participants)", participants_.size());
    for (auto& participant : participants_) {
        if (participant.onBegin)
            participant.onBegin();
    }
}

void WorldLifecycleCoordinator::endWorld() {
    FABRIC_LOG_INFO("WorldLifecycleCoordinator: endWorld ({} participants)", participants_.size());
    for (auto it = participants_.rbegin(); it != participants_.rend(); ++it) {
        if (it->onEnd)
            it->onEnd();
    }

    // Bulk-delete all WorldScoped entities
    if (world_) {
        world_->get().delete_with<WorldScoped>();
    }
}

size_t WorldLifecycleCoordinator::participantCount() const {
    return participants_.size();
}

} // namespace fabric
