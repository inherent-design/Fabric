#pragma once
#include "recurse/simulation/ChunkActivityTracker.hh"
#include "recurse/simulation/SimulationGrid.hh"
#include <string>

namespace fabric::world {

using simulation::ChunkPos;
using simulation::SimulationGrid;

class GeneratorInterface {
  public:
    virtual ~GeneratorInterface() = default;
    virtual void generate(SimulationGrid& grid, ChunkPos pos) = 0;
    virtual std::string name() const = 0;
};

} // namespace fabric::world
