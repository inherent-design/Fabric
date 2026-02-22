#pragma once

#include <flecs.h>

namespace fabric {

// Plain structs for Flecs component registration.
// Intentionally decoupled from Spatial.hh templates to keep ECS components POD.
struct Position {
    float x, y, z;
};

struct Rotation {
    float x, y, z, w; // quaternion (x,y,z,w)
};

struct Scale {
    float x, y, z;
};

struct BoundingBox {
    float minX, minY, minZ;
    float maxX, maxY, maxZ;
};

// Tag component for entities that are part of the scene graph
struct SceneEntity {};

// Component for entities that have a renderable (draw call data)
struct Renderable {
    uint64_t sortKey;
};

// Flecs world wrapper with RAII lifecycle management
class World {
public:
    World();
    ~World();

    World(const World&) = delete;
    World& operator=(const World&) = delete;
    World(World&&) noexcept;
    World& operator=(World&&) noexcept;

    flecs::world& get();
    const flecs::world& get() const;

    // Advance the world by deltaTime (runs all registered systems)
    bool progress(float deltaTime = 0.0f);

    // Register Position, Rotation, Scale, BoundingBox, SceneEntity, Renderable
    void registerCoreComponents();

    // Create a scene entity with Position + Rotation + Scale + SceneEntity tag
    flecs::entity createSceneEntity(const char* name = nullptr);

    // Create a child entity (ChildOf relationship) with scene components
    flecs::entity createChildEntity(flecs::entity parent, const char* name = nullptr);

private:
    flecs::world* world_;
};

} // namespace fabric
