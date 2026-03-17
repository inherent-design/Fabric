#pragma once

#include "fabric/core/SystemBase.hh"
#include "recurse/simulation/ChunkState.hh"
#include <cstddef>
#include <memory>
#include <tuple>
#include <vector>

namespace fabric {
class EventDispatcher;
class JobScheduler;
} // namespace fabric

namespace recurse::simulation {
class ChunkActivityTracker;
class FallingSandSystem;
class GhostCellManager;
class MaterialRegistry;
class SimulationGrid;
class ChangeVelocityTracker;
class VoxelSimulationSystem;
} // namespace recurse::simulation

namespace recurse::systems {

class ChunkPipelineSystem;
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

    void onWorldBegin();
    void onWorldEnd();

    void setWorldSeed(int64_t seed);

    /// Read-only access for VoxelMeshingSystem, ChunkPipelineSystem, DebugOverlaySystem
    recurse::simulation::SimulationGrid& simulationGrid();
    const recurse::simulation::SimulationGrid& simulationGrid() const;
    recurse::simulation::ChunkActivityTracker& activityTracker();
    const recurse::simulation::ChunkActivityTracker& activityTracker() const;
    const recurse::simulation::MaterialRegistry& materials() const;

    /// Generate terrain for a single chunk into the simulation grid.
    /// Called by ChunkPipelineSystem during streaming load.
    void generateChunk(int cx, int cy, int cz);

    /// Parallel generation of multiple chunks via JobScheduler.
    /// Pre-materializes on calling thread, dispatches parallel gen, finalizes sequentially.
    void generateChunksBatch(const std::vector<std::tuple<int, int, int>>& chunks);

    /// Generate initial world region (5x3x5 chunks).
    /// Called by MainMenuSystem when world type is selected.
    void generateInitialWorld();

    /// Reset world state (clear all chunks, tracker).
    /// Called before starting a new world.
    void resetWorld();

    /// Remove an active chunk (Active -> Draining -> erase).
    void removeActiveChunk(recurse::simulation::ChunkRef<recurse::simulation::Active> ref);

    /// Cancel and remove a generating chunk (direct erase, no Draining).
    void cancelChunk(recurse::simulation::ChunkRef<recurse::simulation::Generating> ref);

    /// Remove a chunk by coordinate (runtime dispatch to typed variant).
    /// Prefer removeActiveChunk/cancelChunk when the state is known.
    void removeChunk(int cx, int cy, int cz);

    /// Access the underlying JobScheduler (owned by the inner simulation system).
    fabric::JobScheduler& scheduler();

    recurse::simulation::FallingSandSystem& fallingSandSystem();
    const recurse::simulation::FallingSandSystem& fallingSandSystem() const;
    recurse::simulation::GhostCellManager& ghostCellManager();
    const recurse::simulation::GhostCellManager& ghostCellManager() const;
    int64_t worldSeed() const;

    const recurse::simulation::ChangeVelocityTracker& velocityTracker() const;

    /// Number of chunks actively simulated last tick (for debug overlay)
    size_t activeChunkCount() const { return lastActiveCount_; }

  private:
    TerrainSystem* terrain_ = nullptr;
    fabric::EventDispatcher* dispatcher_ = nullptr;
    std::unique_ptr<recurse::simulation::VoxelSimulationSystem> fabSim_;
    size_t lastActiveCount_ = 0;

    struct GenStats {
        int batchCalls = 0;
        int chunksGenerated = 0;
        int frames = 0;
    };
    GenStats genStats_{};
    static constexpr int K_GEN_LOG_INTERVAL = 60;
};

} // namespace recurse::systems
