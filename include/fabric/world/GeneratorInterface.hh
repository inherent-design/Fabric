#pragma once
#include "recurse/simulation/ChunkActivityTracker.hh"
#include "recurse/simulation/SimulationGrid.hh"
#include <string>

namespace fabric::world {

using recurse::simulation::ChunkCoord;
using recurse::simulation::SimulationGrid;

class GeneratorInterface {
  public:
    virtual ~GeneratorInterface() = default;
    virtual void generate(SimulationGrid& grid, ChunkCoord pos) = 0;
    virtual std::string name() const = 0;
};

} // namespace fabric::world
