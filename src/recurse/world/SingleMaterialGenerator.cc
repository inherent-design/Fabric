#include "recurse/world/SingleMaterialGenerator.hh"
#include "recurse/simulation/SimulationGrid.hh"

namespace recurse {

using namespace simulation;

SingleMaterialGenerator::SingleMaterialGenerator(VoxelCell cell) : cell_(cell) {}

void SingleMaterialGenerator::generate(SimulationGrid& grid, int cx, int cy, int cz) {
    grid.fillChunk(cx, cy, cz, cell_);
}

} // namespace recurse
