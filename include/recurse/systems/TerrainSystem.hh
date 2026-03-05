#pragma once

#include "fabric/core/FieldLayer.hh"
#include "fabric/core/Rendering.hh"
#include "fabric/core/SystemBase.hh"
#include <memory>

namespace fabric::simulation {
class SimulationGrid;
} // namespace fabric::simulation

namespace recurse {
template <typename T> class ChunkedGrid;
class WorldGenerator;
} // namespace recurse

namespace recurse::systems {

/// Owns SimulationGrid and WorldGenerator. Init-only system.
/// init() generates the initial world state using the configured WorldGenerator.
/// VoxelSimulationSystem takes over for ongoing evolution.
/// fixedUpdate is a no-op.
class TerrainSystem : public fabric::System<TerrainSystem> {
  public:
    TerrainSystem();
    ~TerrainSystem() override;

    void init(fabric::AppContext& ctx) override;
    void shutdown() override;
    void fixedUpdate(fabric::AppContext& ctx, float fixedDt) override;
    void configureDependencies() override;

    /// Generate terrain for a single chunk (called by ChunkPipelineSystem on load)
    void generateChunk(int cx, int cy, int cz);

    // VP0+ accessors
    fabric::simulation::SimulationGrid& simulationGrid();
    const fabric::simulation::SimulationGrid& simulationGrid() const;
    WorldGenerator& worldGenerator() { return *worldGen_; }
    void setWorldGenerator(std::unique_ptr<WorldGenerator> gen);

    /// Sync density field from an external SimulationGrid (e.g., VoxelSimulationSystem's grid)
    void syncDensityFromGrid(const fabric::simulation::SimulationGrid& grid);

    // Deprecated: old field accessors kept for compile compat with legacy systems.
    // These return empty default-constructed fields -- callers are dead code paths.
    fabric::DensityField& density() { return density_; }
    const fabric::DensityField& density() const { return density_; }
    fabric::EssenceField& essence() { return essence_; }
    const fabric::EssenceField& essence() const { return essence_; }
    ChunkedGrid<float>& densityGrid() { return density_.grid(); }
    const ChunkedGrid<float>& densityGrid() const { return density_.grid(); }
    ChunkedGrid<fabric::Vector4<float, fabric::Space::World>>& essenceGrid() { return essence_.grid(); }
    const ChunkedGrid<fabric::Vector4<float, fabric::Space::World>>& essenceGrid() const { return essence_.grid(); }

  private:
    std::unique_ptr<fabric::simulation::SimulationGrid> simGrid_;
    std::unique_ptr<WorldGenerator> worldGen_;

    // Deprecated stubs
    fabric::DensityField density_;
    fabric::EssenceField essence_;

    // Sync density field from simulation grid for collision detection
    void syncDensityFromSimulation();
};

} // namespace recurse::systems
