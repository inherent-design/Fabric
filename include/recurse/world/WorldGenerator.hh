#pragma once

#include <cstdint>
#include <string>

namespace recurse::simulation {
class SimulationGrid;
} // namespace recurse::simulation

namespace recurse {

/// Abstract world generator interface. Implementations fill a SimulationGrid
/// region with initial voxel data. Swappable without modifying TerrainSystem.
class WorldGenerator {
  public:
    virtual ~WorldGenerator() = default;

    /// Fill the given chunk of the SimulationGrid with initial voxel data.
    /// Called once per chunk during world init or chunk streaming load.
    virtual void generate(recurse::simulation::SimulationGrid& grid, int cx, int cy, int cz) = 0;

    /// Return the material ID at a single world coordinate.
    /// Used by LOD direct generation (E-3) to fill LOD sections without
    /// allocating full chunk data. Subclasses should override for efficiency;
    /// the default generates an entire chunk and reads the target cell.
    virtual uint16_t sampleMaterial(int wx, int wy, int wz) const;

    /// Conservative upper bound on the maximum Y coordinate where visible
    /// material (non-air) exists in the chunk column at (cx, cz).
    /// Used to skip LOD generation for all-air and all-underground chunks.
    /// Default returns 1024 (never skip for unknown generators).
    virtual int maxSurfaceHeight(int cx, int cz) const;

    /// Human-readable name for logging
    virtual std::string name() const = 0;
};

} // namespace recurse
