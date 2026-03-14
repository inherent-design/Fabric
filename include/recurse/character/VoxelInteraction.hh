#pragma once

#include "fabric/core/Event.hh"
#include "fabric/render/Rendering.hh"
#include "recurse/persistence/ChangeSource.hh"
#include "recurse/simulation/SimulationGrid.hh"
#include "recurse/simulation/VoxelMaterial.hh"
#include "recurse/world/VoxelRaycast.hh"

#include <cstring>
#include <vector>

namespace recurse {

// Event name for voxel data changes (formerly in ChunkMeshManager.hh)
inline constexpr const char* K_VOXEL_CHANGED_EVENT = "voxel_changed";

/// Per-voxel change detail attached to K_VOXEL_CHANGED_EVENT via setAnyData.
/// Carried by player place/destroy events and physics simulation events.
/// Generation events omit this (snapshot system covers; massive volume).
struct VoxelChangeDetail {
    int vx, vy, vz;
    uint32_t oldCell;
    uint32_t newCell;
    int32_t playerId;
    ChangeSource source;
};

/// Emit a chunk-level voxel-changed event (no per-voxel detail).
inline void emitVoxelChanged(fabric::EventDispatcher& dispatcher, int cx, int cy, int cz) {
    fabric::Event e(K_VOXEL_CHANGED_EVENT, "VoxelInteraction");
    e.setData("cx", cx);
    e.setData("cy", cy);
    e.setData("cz", cz);
    dispatcher.dispatchEvent(e);
}

/// Emit a voxel-changed event with a single per-voxel change detail.
inline void emitVoxelChanged(fabric::EventDispatcher& dispatcher, int cx, int cy, int cz,
                             const VoxelChangeDetail& detail) {
    fabric::Event e(K_VOXEL_CHANGED_EVENT, "VoxelInteraction");
    e.setData("cx", cx);
    e.setData("cy", cy);
    e.setData("cz", cz);
    e.setAnyData("detail", std::vector<VoxelChangeDetail>{detail});
    dispatcher.dispatchEvent(e);
}

/// Emit a voxel-changed event with multiple per-voxel details (batch).
inline void emitVoxelChanged(fabric::EventDispatcher& dispatcher, int cx, int cy, int cz,
                             std::vector<VoxelChangeDetail> details) {
    fabric::Event e(K_VOXEL_CHANGED_EVENT, "VoxelInteraction");
    e.setData("cx", cx);
    e.setData("cy", cy);
    e.setData("cz", cz);
    e.setAnyData("detail", std::move(details));
    dispatcher.dispatchEvent(e);
}

// Engine types imported from fabric:: namespace
using fabric::AABB;
using fabric::EventDispatcher;
using recurse::simulation::MaterialId;
using recurse::simulation::SimulationGrid;
using recurse::simulation::VoxelCell;

struct InteractionResult {
    bool success;
    int x, y, z;
    int cx, cy, cz;
};

class VoxelInteraction {
  public:
    VoxelInteraction(SimulationGrid& grid, EventDispatcher& dispatcher);

    // Place voxel adjacent to hit face
    InteractionResult createMatter(const VoxelHit& hit,
                                   MaterialId materialId = recurse::simulation::material_ids::SAND);

    // Remove voxel at hit position
    InteractionResult destroyMatter(const VoxelHit& hit);

    // Raycast + create in one call
    InteractionResult createMatterAt(float ox, float oy, float oz, float dx, float dy, float dz,
                                     MaterialId materialId = recurse::simulation::material_ids::SAND,
                                     float maxDistance = 10.0f);

    // Raycast + destroy in one call
    InteractionResult destroyMatterAt(float ox, float oy, float oz, float dx, float dy, float dz,
                                      float maxDistance = 10.0f);

    // Check if placing at position would overlap an AABB (player push-out check)
    static bool wouldOverlap(int vx, int vy, int vz, const AABB& playerBounds);

  private:
    SimulationGrid& grid_;
    EventDispatcher& dispatcher_;
};

} // namespace recurse
