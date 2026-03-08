#include "fabric/world/LayeredGenerator.hh"
#include "fabric/world/ChunkedGrid.hh"

namespace fabric::world {

using namespace fabric::simulation;

LayeredGenerator::LayeredGenerator(std::vector<LayerDef> layers) : layers_(std::move(layers)) {}

void LayeredGenerator::generate(SimulationGrid& grid, ChunkPos pos) {
    int baseY = pos.y * kChunkSize;

    // Default fill is Air
    grid.fillChunk(pos.x, pos.y, pos.z, VoxelCell{});

    for (int lz = 0; lz < kChunkSize; ++lz) {
        for (int ly = 0; ly < kChunkSize; ++ly) {
            int worldY = baseY + ly;

            // Find last matching layer for this worldY
            const LayerDef* match = nullptr;
            for (const auto& layer : layers_) {
                if (worldY >= layer.minY && worldY <= layer.maxY)
                    match = &layer;
            }

            if (match) {
                for (int lx = 0; lx < kChunkSize; ++lx) {
                    int wx = pos.x * kChunkSize + lx;
                    int wz = pos.z * kChunkSize + lz;
                    grid.writeCell(wx, worldY, wz, match->cell);
                }
            }
        }
    }
}

} // namespace fabric::world
