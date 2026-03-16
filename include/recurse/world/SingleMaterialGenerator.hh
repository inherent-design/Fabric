#pragma once

#include "recurse/simulation/VoxelMaterial.hh"
#include "recurse/world/WorldGenerator.hh"

namespace recurse {

class SingleMaterialGenerator : public WorldGenerator {
  public:
    explicit SingleMaterialGenerator(simulation::VoxelCell cell);
    void generate(simulation::SimulationGrid& grid, int cx, int cy, int cz) override;
    std::string name() const override { return "SingleMaterial"; }

  private:
    simulation::VoxelCell cell_;
};

} // namespace recurse
