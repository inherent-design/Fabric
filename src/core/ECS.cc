#include "fabric/core/ECS.hh"

#include <utility>

namespace fabric {

World::World() : world_(new flecs::world()) {}

World::~World() {
    delete world_;
}

World::World(World&& other) noexcept : world_(other.world_) {
    other.world_ = nullptr;
}

World& World::operator=(World&& other) noexcept {
    if (this != &other) {
        delete world_;
        world_ = other.world_;
        other.world_ = nullptr;
    }
    return *this;
}

flecs::world& World::get() {
    return *world_;
}

const flecs::world& World::get() const {
    return *world_;
}

bool World::progress(float deltaTime) {
    return world_->progress(deltaTime);
}

void World::registerCoreComponents() {
    world_->component<Position>("Position");
    world_->component<Rotation>("Rotation");
    world_->component<Scale>("Scale");
    world_->component<BoundingBox>("BoundingBox");
}

} // namespace fabric
