#include "recurse/world/TestWorldGenerator.hh"

#include "fabric/world/ChunkedGrid.hh"
#include "recurse/simulation/SimulationGrid.hh"
#include "recurse/simulation/VoxelMaterial.hh"

namespace recurse {

using fabric::K_CHUNK_SIZE;

// -- FlatWorldGenerator -------------------------------------------------------

FlatWorldGenerator::FlatWorldGenerator(int groundLevel) : groundLevel_(groundLevel) {}

void FlatWorldGenerator::generate(recurse::simulation::SimulationGrid& grid, int cx, int cy, int cz) {
    using namespace recurse::simulation;

    int baseY = cy * K_CHUNK_SIZE;
    int topY = baseY + K_CHUNK_SIZE - 1;

    // Entirely above ground: all air (default), nothing to do
    if (baseY >= groundLevel_)
        return;

    // Entirely below ground: fill with stone sentinel (no allocation)
    if (topY < groundLevel_) {
        grid.fillChunk(cx, cy, cz, VoxelCell{material_ids::STONE});
        return;
    }

    // Mixed chunk: per-voxel
    int baseX = cx * K_CHUNK_SIZE;
    int baseZ = cz * K_CHUNK_SIZE;
    for (int lz = 0; lz < K_CHUNK_SIZE; ++lz) {
        for (int ly = 0; ly < K_CHUNK_SIZE; ++ly) {
            int wy = baseY + ly;
            if (wy >= groundLevel_)
                break;
            for (int lx = 0; lx < K_CHUNK_SIZE; ++lx) {
                grid.writeCell(baseX + lx, wy, baseZ + lz, VoxelCell{material_ids::STONE});
            }
        }
    }
}

uint16_t FlatWorldGenerator::sampleMaterial(int /*wx*/, int wy, int /*wz*/) const {
    return (wy < groundLevel_) ? simulation::material_ids::STONE : simulation::material_ids::AIR;
}

int FlatWorldGenerator::maxSurfaceHeight(int /*cx*/, int /*cz*/) const {
    return groundLevel_;
}

// -- LayeredWorldGenerator ----------------------------------------------------

LayeredWorldGenerator::LayeredWorldGenerator(int stoneLevel, int sandDepth)
    : stoneLevel_(stoneLevel), sandDepth_(sandDepth) {}

void LayeredWorldGenerator::generate(recurse::simulation::SimulationGrid& grid, int cx, int cy, int cz) {
    using namespace recurse::simulation;

    int baseY = cy * K_CHUNK_SIZE;
    int topY = baseY + K_CHUNK_SIZE - 1;
    int totalGround = stoneLevel_ + sandDepth_;

    // Entirely above all terrain: air, nothing to do
    if (baseY >= totalGround)
        return;

    // Entirely below stone level: all stone
    if (topY < stoneLevel_) {
        grid.fillChunk(cx, cy, cz, VoxelCell{material_ids::STONE});
        return;
    }

    // Mixed or sand-only chunk: per-voxel
    int baseX = cx * K_CHUNK_SIZE;
    int baseZ = cz * K_CHUNK_SIZE;
    for (int lz = 0; lz < K_CHUNK_SIZE; ++lz) {
        for (int ly = 0; ly < K_CHUNK_SIZE; ++ly) {
            int wy = baseY + ly;
            if (wy >= totalGround)
                break;
            MaterialId mat = (wy < stoneLevel_) ? material_ids::STONE : material_ids::SAND;
            for (int lx = 0; lx < K_CHUNK_SIZE; ++lx) {
                grid.writeCell(baseX + lx, wy, baseZ + lz, VoxelCell{mat});
            }
        }
    }
}

uint16_t LayeredWorldGenerator::sampleMaterial(int /*wx*/, int wy, int /*wz*/) const {
    if (wy < stoneLevel_)
        return simulation::material_ids::STONE;
    if (wy < stoneLevel_ + sandDepth_)
        return simulation::material_ids::SAND;
    return simulation::material_ids::AIR;
}

int LayeredWorldGenerator::maxSurfaceHeight(int /*cx*/, int /*cz*/) const {
    return stoneLevel_ + sandDepth_;
}

} // namespace recurse
