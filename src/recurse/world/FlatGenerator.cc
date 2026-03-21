#include "recurse/world/FlatGenerator.hh"
#include "recurse/simulation/CellAccessors.hh"
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
        VoxelCell stone = makeCell(static_cast<uint8_t>(material_ids::STONE), Phase::Solid, 200);
        grid.fillChunk(cx, cy, cz, stone);
        return;
    }

    VoxelCell stone = makeCell(static_cast<uint8_t>(material_ids::STONE), Phase::Solid, 200);
    grid.fillChunk(cx, cy, cz, stone);

    for (int lz = 0; lz < K_CHUNK_SIZE; ++lz) {
        for (int ly = 0; ly < K_CHUNK_SIZE; ++ly) {
            int worldY = baseY + ly;
            if (worldY == surfaceHeight_) {
                VoxelCell dirt = makeCell(static_cast<uint8_t>(material_ids::DIRT), Phase::Solid, 150);
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
