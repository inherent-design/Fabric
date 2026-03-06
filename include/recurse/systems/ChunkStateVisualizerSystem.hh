#pragma once

#include "fabric/core/SystemBase.hh"

namespace fabric {
class AppContext;
}

namespace recurse::systems {

class VoxelSimulationSystem;
class ChunkPipelineSystem;
class DebugDraw;

/// Visualizes chunk states via colored wireframe overlays.
/// Toggle with F12 key.
/// Colors:
///   - Sleeping: Gray-blue (idle)
///   - Active: Green (simulating)
///   - BoundaryDirty: Yellow-orange (needs check)
class ChunkStateVisualizerSystem : public fabric::System<ChunkStateVisualizerSystem> {
  public:
    ChunkStateVisualizerSystem() = default;
    ~ChunkStateVisualizerSystem() override = default;

    void init(fabric::AppContext& ctx) override;
    void shutdown() override;
    void render(fabric::AppContext& ctx) override;
    void configureDependencies() override;

  private:
    VoxelSimulationSystem* simSystem_ = nullptr;
    ChunkPipelineSystem* pipeline_ = nullptr;
    DebugDraw* debugDraw_ = nullptr;
};

} // namespace recurse::systems
