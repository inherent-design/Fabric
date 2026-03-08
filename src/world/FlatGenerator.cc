#include "fabric/world/FlatGenerator.hh"
#include "fabric/world/ChunkedGrid.hh"

namespace fabric::world {

using namespace fabric::simulation;

FlatGenerator::FlatGenerator(int surfaceHeight) : surfaceHeight_(surfaceHeight) {}

void FlatGenerator::generate(SimulationGrid& grid, ChunkPos pos) {
    int baseY = pos.y * kChunkSize;
    int topY = baseY + kChunkSize - 1;

    // Entire chunk above surface -- leave as Air (default sentinel)
    if (baseY > surfaceHeight_) {
        grid.fillChunk(pos.x, pos.y, pos.z, VoxelCell{});
        return;
    }

    // Entire chunk below surface -- all Stone
    if (topY < surfaceHeight_) {
        VoxelCell stone;
        stone.materialId = material_ids::STONE;
        grid.fillChunk(pos.x, pos.y, pos.z, stone);
        return;
    }

    // Mixed chunk: contains the surface layer
    // Pre-fill with Stone (most cells below surface), then overwrite
    VoxelCell stone;
    stone.materialId = material_ids::STONE;
    grid.fillChunk(pos.x, pos.y, pos.z, stone);

    for (int lz = 0; lz < kChunkSize; ++lz) {
        for (int ly = 0; ly < kChunkSize; ++ly) {
            int worldY = baseY + ly;
            if (worldY == surfaceHeight_) {
                // Surface layer = Dirt
                VoxelCell dirt;
                dirt.materialId = material_ids::DIRT;
                for (int lx = 0; lx < kChunkSize; ++lx) {
                    int wx = pos.x * kChunkSize + lx;
                    int wz = pos.z * kChunkSize + lz;
                    grid.writeCell(wx, worldY, wz, dirt);
                }
            } else if (worldY > surfaceHeight_) {
                // Above surface = Air
                VoxelCell air;
                for (int lx = 0; lx < kChunkSize; ++lx) {
                    int wx = pos.x * kChunkSize + lx;
                    int wz = pos.z * kChunkSize + lz;
                    grid.writeCell(wx, worldY, wz, air);
                }
            }
            // Below surface: Stone fill already set
        }
    }
}

} // namespace fabric::world
