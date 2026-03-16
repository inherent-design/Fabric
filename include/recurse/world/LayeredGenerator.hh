#pragma once

#include "recurse/simulation/VoxelMaterial.hh"
#include "recurse/world/WorldGenerator.hh"
#include <vector>

namespace recurse {

struct LayerDef {
    simulation::VoxelCell cell;
    int minY;
    int maxY;
};

class LayeredGenerator : public WorldGenerator {
  public:
    explicit LayeredGenerator(std::vector<LayerDef> layers);
    void generate(simulation::SimulationGrid& grid, int cx, int cy, int cz) override;
    std::string name() const override { return "Layered"; }

  private:
    std::vector<LayerDef> layers_;
};

} // namespace recurse
