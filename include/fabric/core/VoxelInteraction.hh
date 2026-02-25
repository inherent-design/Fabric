#pragma once

#include "fabric/core/Event.hh"
#include "fabric/core/FieldLayer.hh"
#include "fabric/core/Rendering.hh"
#include "fabric/core/VoxelRaycast.hh"

namespace fabric {

struct InteractionResult {
    bool success;
    int x, y, z;
    int cx, cy, cz;
};

class VoxelInteraction {
  public:
    VoxelInteraction(DensityField& density, EssenceField& essence, EventDispatcher& dispatcher);

    // Place voxel adjacent to hit face
    InteractionResult createMatter(const VoxelHit& hit, float density = 1.0f,
                                   const Vector4<float, Space::World>& essenceColor = {0.5f, 0.5f, 0.5f, 1.0f});

    // Remove voxel at hit position
    InteractionResult destroyMatter(const VoxelHit& hit);

    // Raycast + create in one call
    InteractionResult createMatterAt(const ChunkedGrid<float>& grid, float ox, float oy, float oz, float dx, float dy,
                                     float dz, float density = 1.0f,
                                     const Vector4<float, Space::World>& essenceColor = {0.5f, 0.5f, 0.5f, 1.0f},
                                     float maxDistance = 10.0f);

    // Raycast + destroy in one call
    InteractionResult destroyMatterAt(const ChunkedGrid<float>& grid, float ox, float oy, float oz, float dx, float dy,
                                      float dz, float maxDistance = 10.0f);

    // Check if placing at position would overlap an AABB (player push-out check)
    static bool wouldOverlap(int vx, int vy, int vz, const AABB& playerBounds);

  private:
    DensityField& density_;
    EssenceField& essence_;
    EventDispatcher& dispatcher_;
};

} // namespace fabric
