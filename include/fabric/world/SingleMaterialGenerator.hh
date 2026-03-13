#pragma once
#include "fabric/world/GeneratorInterface.hh"
#include "recurse/simulation/VoxelMaterial.hh"

namespace fabric::world {

class SingleMaterialGenerator : public GeneratorInterface {
  public:
    explicit SingleMaterialGenerator(recurse::simulation::VoxelCell cell);
    void generate(SimulationGrid& grid, ChunkCoord pos) override;
    std::string name() const override { return "SingleMaterial"; }

  private:
    recurse::simulation::VoxelCell cell_;
};

} // namespace fabric::world
