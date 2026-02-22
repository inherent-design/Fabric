#include "fabric/core/ECS.hh"
#include "fabric/utils/Profiler.hh"

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
    FABRIC_ZONE_SCOPED_N("ECS::progress");
    return world_->progress(deltaTime);
}

void World::registerCoreComponents() {
    world_->component<Position>("Position");
    world_->component<Rotation>("Rotation");
    world_->component<Scale>("Scale");
    world_->component<BoundingBox>("BoundingBox");
    world_->component<SceneEntity>("SceneEntity");
    world_->component<Renderable>("Renderable");
}

flecs::entity World::createSceneEntity(const char* name) {
    auto builder = name ? world_->entity(name) : world_->entity();
    return builder
        .set<Position>({0.0f, 0.0f, 0.0f})
        .set<Rotation>({0.0f, 0.0f, 0.0f, 1.0f})
        .set<Scale>({1.0f, 1.0f, 1.0f})
        .add<SceneEntity>();
}

flecs::entity World::createChildEntity(flecs::entity parent, const char* name) {
    auto builder = name ? world_->entity(name) : world_->entity();
    return builder
        .child_of(parent)
        .set<Position>({0.0f, 0.0f, 0.0f})
        .set<Rotation>({0.0f, 0.0f, 0.0f, 1.0f})
        .set<Scale>({1.0f, 1.0f, 1.0f})
        .add<SceneEntity>();
}

} // namespace fabric
