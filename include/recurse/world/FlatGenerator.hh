#pragma once

#include "recurse/world/WorldGenerator.hh"

namespace recurse {

class FlatGenerator : public WorldGenerator {
  public:
    explicit FlatGenerator(int surfaceHeight = 16);
    void generate(simulation::SimulationGrid& grid, int cx, int cy, int cz) override;
    std::string name() const override { return "Flat"; }

  private:
    int surfaceHeight_;
};

} // namespace recurse
