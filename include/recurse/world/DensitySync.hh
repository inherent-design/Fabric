#pragma once

#include "fabric/simulation/SimulationGrid.hh"
#include "fabric/simulation/VoxelMaterial.hh"
#include "recurse/world/ChunkedGrid.hh"

namespace recurse {

/// Sync density values from a SimulationGrid chunk to a ChunkedGrid<float>.
/// Solid materials get density 1.0f, AIR gets 0.0f.
/// Handles both materialized buffers and sentinel (fill-value) chunks.
inline void syncChunkDensity(const fabric::simulation::SimulationGrid& simGrid, ChunkedGrid<float>& densityGrid, int cx,
                             int cy, int cz) {
    using namespace fabric::simulation;

    constexpr int K_CHUNK_SIZE = 32;
    int baseX = cx * K_CHUNK_SIZE;
    int baseY = cy * K_CHUNK_SIZE;
    int baseZ = cz * K_CHUNK_SIZE;

    const auto* readBuf = simGrid.readBuffer(cx, cy, cz);
    if (readBuf) {
        for (int lz = 0; lz < K_CHUNK_SIZE; ++lz) {
            for (int ly = 0; ly < K_CHUNK_SIZE; ++ly) {
                for (int lx = 0; lx < K_CHUNK_SIZE; ++lx) {
                    size_t idx = static_cast<size_t>(lx + ly * K_CHUNK_SIZE + lz * K_CHUNK_SIZE * K_CHUNK_SIZE);
                    const VoxelCell& cell = (*readBuf)[idx];
                    float density = (cell.materialId == material_ids::AIR) ? 0.0f : 1.0f;
                    densityGrid.set(baseX + lx, baseY + ly, baseZ + lz, density);
                }
            }
        }
    } else if (simGrid.hasChunk(cx, cy, cz)) {
        VoxelCell fillValue = simGrid.getChunkFillValue(cx, cy, cz);
        float density = (fillValue.materialId == material_ids::AIR) ? 0.0f : 1.0f;
        for (int lz = 0; lz < K_CHUNK_SIZE; ++lz) {
            for (int ly = 0; ly < K_CHUNK_SIZE; ++ly) {
                for (int lx = 0; lx < K_CHUNK_SIZE; ++lx) {
                    densityGrid.set(baseX + lx, baseY + ly, baseZ + lz, density);
                }
            }
        }
    }
}

} // namespace recurse
