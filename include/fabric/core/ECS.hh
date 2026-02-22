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

    // Register Position, Rotation, Scale, BoundingBox as named components
    void registerCoreComponents();

private:
    flecs::world* world_;
};

} // namespace fabric
