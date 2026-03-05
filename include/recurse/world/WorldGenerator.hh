#pragma once

#include <string>

namespace fabric::simulation {
class SimulationGrid;
} // namespace fabric::simulation

namespace recurse {

/// Abstract world generator interface. Implementations fill a SimulationGrid
/// region with initial voxel data. Swappable without modifying TerrainSystem.
class WorldGenerator {
  public:
    virtual ~WorldGenerator() = default;

    /// Fill the given chunk of the SimulationGrid with initial voxel data.
    /// Called once per chunk during world init or chunk streaming load.
    virtual void generate(fabric::simulation::SimulationGrid& grid, int cx, int cy, int cz) = 0;

    /// Human-readable name for logging
    virtual std::string name() const = 0;
};

} // namespace recurse
