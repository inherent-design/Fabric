#include "recurse/world/TestWorldGenerator.hh"

#include "fabric/simulation/SimulationGrid.hh"
#include "recurse/world/ChunkedGrid.hh"

namespace recurse {

// -- FlatWorldGenerator -------------------------------------------------------

FlatWorldGenerator::FlatWorldGenerator(int groundLevel) : groundLevel_(groundLevel) {}

void FlatWorldGenerator::generate(fabric::simulation::SimulationGrid& grid, int cx, int cy, int cz) {
    using namespace fabric::simulation;

    int baseY = cy * kChunkSize;
    int topY = baseY + kChunkSize - 1;

    // Entirely above ground: all air (default), nothing to do
    if (baseY >= groundLevel_)
        return;

    // Entirely below ground: fill with stone sentinel (no allocation)
    if (topY < groundLevel_) {
        grid.fillChunk(cx, cy, cz, VoxelCell{MaterialIds::Stone});
        return;
    }

    // Mixed chunk: per-voxel
    int baseX = cx * kChunkSize;
    int baseZ = cz * kChunkSize;
    for (int lz = 0; lz < kChunkSize; ++lz) {
        for (int ly = 0; ly < kChunkSize; ++ly) {
            int wy = baseY + ly;
            if (wy >= groundLevel_)
                break;
            for (int lx = 0; lx < kChunkSize; ++lx) {
                grid.writeCell(baseX + lx, wy, baseZ + lz, VoxelCell{MaterialIds::Stone});
            }
        }
    }
}

// -- LayeredWorldGenerator ----------------------------------------------------

LayeredWorldGenerator::LayeredWorldGenerator(int stoneLevel, int sandDepth)
    : stoneLevel_(stoneLevel), sandDepth_(sandDepth) {}

void LayeredWorldGenerator::generate(fabric::simulation::SimulationGrid& grid, int cx, int cy, int cz) {
    using namespace fabric::simulation;

    int baseY = cy * kChunkSize;
    int topY = baseY + kChunkSize - 1;
    int totalGround = stoneLevel_ + sandDepth_;

    // Entirely above all terrain: air, nothing to do
    if (baseY >= totalGround)
        return;

    // Entirely below stone level: all stone
    if (topY < stoneLevel_) {
        grid.fillChunk(cx, cy, cz, VoxelCell{MaterialIds::Stone});
        return;
    }

    // Mixed or sand-only chunk: per-voxel
    int baseX = cx * kChunkSize;
    int baseZ = cz * kChunkSize;
    for (int lz = 0; lz < kChunkSize; ++lz) {
        for (int ly = 0; ly < kChunkSize; ++ly) {
            int wy = baseY + ly;
            if (wy >= totalGround)
                break;
            MaterialId mat = (wy < stoneLevel_) ? MaterialIds::Stone : MaterialIds::Sand;
            for (int lx = 0; lx < kChunkSize; ++lx) {
                grid.writeCell(baseX + lx, wy, baseZ + lz, VoxelCell{mat});
            }
        }
    }
}

} // namespace recurse
