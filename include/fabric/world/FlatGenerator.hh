#pragma once
#include "fabric/world/GeneratorInterface.hh"

namespace fabric::world {

class FlatGenerator : public GeneratorInterface {
  public:
    explicit FlatGenerator(int surfaceHeight = 16);
    void generate(SimulationGrid& grid, ChunkPos pos) override;
    std::string name() const override { return "Flat"; }

  private:
    int surfaceHeight_;
};

} // namespace fabric::world
