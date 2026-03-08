#pragma once
#include "fabric/world/GeneratorInterface.hh"
#include "recurse/simulation/VoxelMaterial.hh"
#include <vector>

namespace fabric::world {

struct LayerDef {
    recurse::simulation::VoxelCell cell;
    int minY;
    int maxY;
};

class LayeredGenerator : public GeneratorInterface {
  public:
    explicit LayeredGenerator(std::vector<LayerDef> layers);
    void generate(SimulationGrid& grid, ChunkPos pos) override;
    std::string name() const override { return "Layered"; }

  private:
    std::vector<LayerDef> layers_;
};

} // namespace fabric::world
