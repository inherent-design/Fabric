#include "recurse/world/FlatGenerator.hh"
#include "recurse/simulation/SimulationGrid.hh"
#include "recurse/simulation/VoxelMaterial.hh"

namespace recurse {

using namespace simulation;

FlatGenerator::FlatGenerator(int surfaceHeight) : surfaceHeight_(surfaceHeight) {}

void FlatGenerator::generate(SimulationGrid& grid, int cx, int cy, int cz) {
    int baseY = cy * K_CHUNK_SIZE;
    int topY = baseY + K_CHUNK_SIZE - 1;

    if (baseY > surfaceHeight_) {
        grid.fillChunk(cx, cy, cz, VoxelCell{});
        return;
    }

    if (topY < surfaceHeight_) {
        VoxelCell stone;
        stone.materialId = material_ids::STONE;
        grid.fillChunk(cx, cy, cz, stone);
        return;
    }

    VoxelCell stone;
    stone.materialId = material_ids::STONE;
    grid.fillChunk(cx, cy, cz, stone);

    for (int lz = 0; lz < K_CHUNK_SIZE; ++lz) {
        for (int ly = 0; ly < K_CHUNK_SIZE; ++ly) {
            int worldY = baseY + ly;
            if (worldY == surfaceHeight_) {
                VoxelCell dirt;
                dirt.materialId = material_ids::DIRT;
                for (int lx = 0; lx < K_CHUNK_SIZE; ++lx) {
                    int wx = cx * K_CHUNK_SIZE + lx;
                    int wz = cz * K_CHUNK_SIZE + lz;
                    grid.writeCell(wx, worldY, wz, dirt);
                }
            } else if (worldY > surfaceHeight_) {
                VoxelCell air;
                for (int lx = 0; lx < K_CHUNK_SIZE; ++lx) {
                    int wx = cx * K_CHUNK_SIZE + lx;
                    int wz = cz * K_CHUNK_SIZE + lz;
                    grid.writeCell(wx, worldY, wz, air);
                }
            }
        }
    }
}

} // namespace recurse
