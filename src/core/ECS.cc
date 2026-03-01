#include "fabric/core/ECS.hh"
#include "fabric/core/Log.hh"
#include "fabric/core/Spatial.hh"
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
    world_->component<LocalToWorld>("LocalToWorld");
    world_->component<SceneEntity>("SceneEntity");
    world_->component<TransparentTag>("TransparentTag");
    world_->component<Renderable>("Renderable");
}

#ifdef FABRIC_ECS_INSPECTOR
void World::enableInspector() {
    // clang-format off
    world_->import<flecs::stats>();  // v21 misparses import as C++20 keyword
    // clang-format on
    world_->set<flecs::Rest>({});
    FABRIC_LOG_INFO("Flecs Explorer available at http://localhost:27750");
}
#endif

void World::updateTransforms() {
    FABRIC_ZONE_SCOPED_N("ECS::updateTransforms");

    // CASCADE query ensures breadth-first order: parents are processed before children.
    // The optional ChildOf term means root entities (no parent) are also matched.
    auto q = world_->query_builder<const Position, const Rotation, const Scale, LocalToWorld>()
                 .with(flecs::ChildOf, flecs::Wildcard)
                 .cascade()
                 .optional()
                 .build();

    q.each([](flecs::entity e, const Position& pos, const Rotation& rot, const Scale& scl, LocalToWorld& ltw) {
        // Compose local transform from Position * Rotation * Scale
        Transform<float> t;
        t.setPosition(Vector3<float, Space::World>(pos.x, pos.y, pos.z));
        t.setRotation(Quaternion<float>(rot.x, rot.y, rot.z, rot.w));
        t.setScale(Vector3<float, Space::World>(scl.x, scl.y, scl.z));
        auto localMatrix = t.getMatrix();

        auto parent = e.parent();
        if (parent.is_valid() && parent.has<LocalToWorld>()) {
            // Parent already processed (CASCADE guarantee): multiply parent * local
            const auto& parentLtw = parent.get<LocalToWorld>();
            Matrix4x4<float> parentMat(parentLtw.matrix);
            auto worldMatrix = parentMat * localMatrix;
            ltw.matrix = worldMatrix.elements;
        } else {
            ltw.matrix = localMatrix.elements;
        }
    });
}

flecs::entity World::createSceneEntity(const char* name) {
    auto builder = name ? world_->entity(name) : world_->entity();
    return builder.set<Position>({0.0f, 0.0f, 0.0f})
        .set<Rotation>({0.0f, 0.0f, 0.0f, 1.0f})
        .set<Scale>({1.0f, 1.0f, 1.0f})
        .set<LocalToWorld>({})
        .add<SceneEntity>();
}

flecs::entity World::createChildEntity(flecs::entity parent, const char* name) {
    auto builder = name ? world_->entity(name) : world_->entity();
    return builder.child_of(parent)
        .set<Position>({0.0f, 0.0f, 0.0f})
        .set<Rotation>({0.0f, 0.0f, 0.0f, 1.0f})
        .set<Scale>({1.0f, 1.0f, 1.0f})
        .set<LocalToWorld>({})
        .add<SceneEntity>();
}

} // namespace fabric
