#pragma once

#include "fabric/core/SystemBase.hh"
#include <cstddef>
#include <memory>

namespace fabric {
class EventDispatcher;
}

namespace fabric::simulation {
class ChunkActivityTracker;
class SimulationGrid;
class VoxelSimulationSystem;
} // namespace fabric::simulation

namespace recurse::systems {

class TerrainSystem;

/// Wraps the fabric-level VoxelSimulationSystem as a System<> in FixedUpdate.
/// Runs after TerrainSystem. Delegates tick() to the fabric orchestration loop.
class VoxelSimulationSystem : public fabric::System<VoxelSimulationSystem> {
  public:
    VoxelSimulationSystem();
    ~VoxelSimulationSystem() override;

    void doInit(fabric::AppContext& ctx) override;
    void doShutdown() override;
    void fixedUpdate(fabric::AppContext& ctx, float fixedDt) override;
    void configureDependencies() override;

    /// Read-only access for VoxelMeshingSystem and ChunkPipelineSystem
    fabric::simulation::SimulationGrid& simulationGrid();
    const fabric::simulation::SimulationGrid& simulationGrid() const;
    fabric::simulation::ChunkActivityTracker& activityTracker();
    const fabric::simulation::ChunkActivityTracker& activityTracker() const;

    /// Generate terrain for a single chunk into the simulation grid.
    /// Called by ChunkPipelineSystem during streaming load.
    void generateChunk(int cx, int cy, int cz);

    /// Generate initial world region (5x3x5 chunks).
    /// Called by MainMenuSystem when world type is selected.
    void generateInitialWorld();

    /// Reset world state (clear all chunks, tracker).
    /// Called before starting a new world.
    void resetWorld();

    /// Remove a chunk from the simulation grid.
    /// Called by ChunkPipelineSystem during streaming unload.
    void removeChunk(int cx, int cy, int cz);

    /// Number of chunks actively simulated last tick (for debug overlay)
    size_t activeChunkCount() const { return lastActiveCount_; }

  private:
    TerrainSystem* terrain_ = nullptr;
    fabric::EventDispatcher* dispatcher_ = nullptr;
    std::unique_ptr<fabric::simulation::VoxelSimulationSystem> fabSim_;
    size_t lastActiveCount_ = 0;
};

} // namespace recurse::systems
