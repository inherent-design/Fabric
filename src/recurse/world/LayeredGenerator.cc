#include "recurse/world/LayeredGenerator.hh"
#include "recurse/simulation/SimulationGrid.hh"
#include "recurse/simulation/VoxelMaterial.hh"

namespace recurse {

using namespace simulation;

LayeredGenerator::LayeredGenerator(std::vector<LayerDef> layers) : layers_(std::move(layers)) {}

void LayeredGenerator::generate(SimulationGrid& grid, int cx, int cy, int cz) {
    int baseY = cy * K_CHUNK_SIZE;

    grid.fillChunk(cx, cy, cz, VoxelCell{});

    for (int lz = 0; lz < K_CHUNK_SIZE; ++lz) {
        for (int ly = 0; ly < K_CHUNK_SIZE; ++ly) {
            int worldY = baseY + ly;

            const LayerDef* match = nullptr;
            for (const auto& layer : layers_) {
                if (worldY >= layer.minY && worldY <= layer.maxY)
                    match = &layer;
            }

            if (match) {
                for (int lx = 0; lx < K_CHUNK_SIZE; ++lx) {
                    int wx = cx * K_CHUNK_SIZE + lx;
                    int wz = cz * K_CHUNK_SIZE + lz;
                    grid.writeCell(wx, worldY, wz, match->cell);
                }
            }
        }
    }
}

} // namespace recurse
