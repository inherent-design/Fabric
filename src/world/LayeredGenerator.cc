#include "fabric/world/LayeredGenerator.hh"
#include "fabric/world/ChunkedGrid.hh"

namespace fabric::world {

using namespace recurse::simulation;

LayeredGenerator::LayeredGenerator(std::vector<LayerDef> layers) : layers_(std::move(layers)) {}

void LayeredGenerator::generate(SimulationGrid& grid, ChunkCoord pos) {
    int baseY = pos.y * K_CHUNK_SIZE;

    // Default fill is Air
    grid.fillChunk(pos.x, pos.y, pos.z, VoxelCell{});

    for (int lz = 0; lz < K_CHUNK_SIZE; ++lz) {
        for (int ly = 0; ly < K_CHUNK_SIZE; ++ly) {
            int worldY = baseY + ly;

            // Find last matching layer for this worldY
            const LayerDef* match = nullptr;
            for (const auto& layer : layers_) {
                if (worldY >= layer.minY && worldY <= layer.maxY)
                    match = &layer;
            }

            if (match) {
                for (int lx = 0; lx < K_CHUNK_SIZE; ++lx) {
                    int wx = pos.x * K_CHUNK_SIZE + lx;
                    int wz = pos.z * K_CHUNK_SIZE + lz;
                    grid.writeCell(wx, worldY, wz, match->cell);
                }
            }
        }
    }
}

} // namespace fabric::world
