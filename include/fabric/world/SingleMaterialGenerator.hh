#pragma once
#include "fabric/simulation/VoxelMaterial.hh"
#include "fabric/world/GeneratorInterface.hh"

namespace fabric::world {

class SingleMaterialGenerator : public GeneratorInterface {
  public:
    explicit SingleMaterialGenerator(fabric::simulation::VoxelCell cell);
    void generate(SimulationGrid& grid, ChunkPos pos) override;
    std::string name() const override { return "SingleMaterial"; }

  private:
    fabric::simulation::VoxelCell cell_;
};

} // namespace fabric::world
