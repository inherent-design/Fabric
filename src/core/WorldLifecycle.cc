#include "fabric/core/WorldLifecycle.hh"
#include "fabric/core/SystemRegistry.hh"
#include "fabric/log/Log.hh"

namespace fabric {

void WorldLifecycleCoordinator::discover(SystemRegistry& registry) {
    participants_.clear();
    registry.forEachInInitOrder([this](SystemBase& system) {
        if (auto* aware = dynamic_cast<WorldAware*>(&system)) {
            participants_.push_back(aware);
        }
    });
    FABRIC_LOG_INFO("WorldLifecycleCoordinator: discovered {} WorldAware participants", participants_.size());
}

void WorldLifecycleCoordinator::beginWorld() {
    FABRIC_LOG_INFO("WorldLifecycleCoordinator: beginWorld ({} participants)", participants_.size());
    for (auto* participant : participants_) {
        participant->onWorldBegin();
    }
}

void WorldLifecycleCoordinator::endWorld() {
    FABRIC_LOG_INFO("WorldLifecycleCoordinator: endWorld ({} participants)", participants_.size());
    for (auto it = participants_.rbegin(); it != participants_.rend(); ++it) {
        (*it)->onWorldEnd();
    }
    // TODO(2p): ecs_delete_with(WorldScoped) here
}

size_t WorldLifecycleCoordinator::participantCount() const {
    return participants_.size();
}

} // namespace fabric
