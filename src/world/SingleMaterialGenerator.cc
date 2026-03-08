#include "fabric/world/SingleMaterialGenerator.hh"

namespace fabric::world {

SingleMaterialGenerator::SingleMaterialGenerator(recurse::simulation::VoxelCell cell) : cell_(cell) {}

void SingleMaterialGenerator::generate(SimulationGrid& grid, ChunkPos pos) {
    grid.fillChunk(pos.x, pos.y, pos.z, cell_);
}

} // namespace fabric::world
