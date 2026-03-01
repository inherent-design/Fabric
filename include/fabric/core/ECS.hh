#pragma once

#include <flecs.h>

#include <array>

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

// World-space transform matrix, updated by CASCADE system from Position/Rotation/Scale hierarchy
struct LocalToWorld {
    std::array<float, 16> matrix = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
};

// Tag component for entities that are part of the scene graph
struct SceneEntity {};

// Tag component for entities that should be rendered in the transparent pass
struct TransparentTag {};

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

    // Register Position, Rotation, Scale, BoundingBox, LocalToWorld, SceneEntity, Renderable
    void registerCoreComponents();

#ifdef FABRIC_ECS_INSPECTOR
    // Enable Flecs REST API + stats for web-based entity inspection.
    // Starts HTTP server on port 27750, browsable at https://www.flecs.dev/explorer
    void enableInspector();
#endif

    // Propagate Position/Rotation/Scale through ChildOf hierarchy into LocalToWorld
    void updateTransforms();

    // Create a scene entity with Position + Rotation + Scale + LocalToWorld + SceneEntity tag
    flecs::entity createSceneEntity(const char* name = nullptr);

    // Create a child entity (ChildOf relationship) with scene components
    flecs::entity createChildEntity(flecs::entity parent, const char* name = nullptr);

  private:
    flecs::world* world_;
};

} // namespace fabric
