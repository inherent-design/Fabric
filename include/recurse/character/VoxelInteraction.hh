#pragma once

#include "fabric/core/Event.hh"
#include "fabric/render/Geometry.hh"
#include "recurse/persistence/ChangeSource.hh"
#include "recurse/simulation/MaterialRegistry.hh"
#include "recurse/simulation/SimulationGrid.hh"
#include "recurse/simulation/VoxelMaterial.hh"
#include "recurse/world/FunctionContracts.hh"
#include "recurse/world/VoxelRaycast.hh"

#include <optional>
#include <vector>

namespace recurse {

// Event name for voxel data changes (formerly in ChunkMeshManager.hh)
inline constexpr const char* K_VOXEL_CHANGED_EVENT = "voxel_changed";

inline void attachWorldChangeEnvelope(fabric::Event& event, WorldChangeEnvelope envelope) {
    event.setAnyData(K_WORLD_CHANGE_ENVELOPE_KEY, std::move(envelope));
}

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
    const auto chunk = fabric::ChunkCoord{cx, cy, cz};
    const auto details = std::vector<VoxelChangeDetail>{detail};
    e.setAnyData("detail", details);
    attachWorldChangeEnvelope(e, makeDetailedChangeEnvelope(chunk, details));
    dispatcher.dispatchEvent(e);
}

/// Emit a voxel-changed event with multiple per-voxel details (batch).
inline void emitVoxelChanged(fabric::EventDispatcher& dispatcher, int cx, int cy, int cz,
                             std::vector<VoxelChangeDetail> details) {
    fabric::Event e(K_VOXEL_CHANGED_EVENT, "VoxelInteraction");
    e.setData("cx", cx);
    e.setData("cy", cy);
    e.setData("cz", cz);
    const auto chunk = fabric::ChunkCoord{cx, cy, cz};
    attachWorldChangeEnvelope(e, makeDetailedChangeEnvelope(chunk, details));
    e.setAnyData("detail", std::move(details));
    dispatcher.dispatchEvent(e);
}

inline void emitChunkChangeSummary(fabric::EventDispatcher& dispatcher, int cx, int cy, int cz, ChangeSource source,
                                   FunctionHistoryMode historyMode = FunctionHistoryMode::ChunkSummary,
                                   FunctionCostClass costClass = FunctionCostClass::ChunkLinear,
                                   std::optional<simulation::ChunkFinalizationCause> finalizationCause = std::nullopt,
                                   FunctionTargetKind targetKind = FunctionTargetKind::Chunk) {
    fabric::Event e(K_VOXEL_CHANGED_EVENT, "VoxelInteraction");
    e.setData("cx", cx);
    e.setData("cy", cy);
    e.setData("cz", cz);
    e.setData("source", static_cast<int>(source));
    attachWorldChangeEnvelope(
        e, makeChunkSummaryChangeEnvelope({cx, cy, cz}, source, targetKind, historyMode, costClass, finalizationCause));
    dispatcher.dispatchEvent(e);
}

// Engine types imported from fabric:: namespace
using fabric::AABB;
using fabric::EventDispatcher;
using recurse::simulation::MaterialId;
using recurse::simulation::SimulationGrid;
using recurse::simulation::VoxelCell;

struct InteractionResult {
    bool success = false;
    int x = 0, y = 0, z = 0;
    int cx = 0, cy = 0, cz = 0;
    VoxelCell newCell{};
    ChangeSource source = ChangeSource::Place;
    int32_t playerId = 0;
};

class VoxelInteraction {
  public:
    VoxelInteraction(SimulationGrid& grid, const simulation::MaterialRegistry& registry);

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
    const simulation::MaterialRegistry& registry_;
};

} // namespace recurse
