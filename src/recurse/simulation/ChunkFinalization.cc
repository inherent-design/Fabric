#include "recurse/simulation/ChunkFinalization.hh"

#include "fabric/world/ChunkCoordUtils.hh"
#include "recurse/simulation/EssenceAssigner.hh"
#include "recurse/simulation/MaterialRegistry.hh"
#include "recurse/simulation/SimulationGrid.hh"
#include "recurse/simulation/VoxelConstants.hh"

namespace recurse::simulation {

namespace {

void notifyNeighbors(ChunkActivityTracker& tracker, const SimulationGrid& grid, int cx, int cy, int cz,
                     NeighborInvalidation mode) {
    if (mode == NeighborInvalidation::None)
        return;

    if (mode == NeighborInvalidation::Face) {
        for (const auto& offset : fabric::K_FACE_NEIGHBORS) {
            const int nx = cx + offset[0];
            const int ny = cy + offset[1];
            const int nz = cz + offset[2];
            if (grid.hasChunk(nx, ny, nz))
                tracker.notifyBoundaryChange(ChunkCoord{nx, ny, nz});
        }
        return;
    }

    for (const auto& offset : fabric::K_FACE_DIAGONAL_NEIGHBORS) {
        const int nx = cx + offset[0];
        const int ny = cy + offset[1];
        const int nz = cz + offset[2];
        if (grid.hasChunk(nx, ny, nz))
            tracker.notifyBoundaryChange(ChunkCoord{nx, ny, nz});
    }
}

} // namespace

void finalizeChunkBuffers(SimulationGrid& grid, int cx, int cy, int cz, const ChunkBufferFinalizationOptions& options) {
    if (options.restorePalette) {
        if (auto* palette = grid.chunkPalette(cx, cy, cz)) {
            palette->clear();
            if (!options.paletteData.empty()) {
                for (size_t base = 0; base + 3 < options.paletteData.size(); base += 4) {
                    palette->addEntryRaw({options.paletteData[base], options.paletteData[base + 1],
                                          options.paletteData[base + 2], options.paletteData[base + 3]});
                }
            } else if (options.materials) {
                if (auto* buffer = grid.writeBuffer(cx, cy, cz))
                    assignEssence(buffer->data(), cx, cy, cz, *options.materials, *palette, 42);
            }
        }
    }

    const int srcBufferIndex = options.sourceBufferIndex >= 0 ? options.sourceBufferIndex : grid.currentWriteIndex();
    grid.syncChunkBuffersFrom(cx, cy, cz, srcBufferIndex);
}

void finalizeChunkActivation(ChunkActivityTracker& tracker, const SimulationGrid& grid, int cx, int cy, int cz,
                             const ChunkActivationOptions& options) {
    const ChunkCoord coord{cx, cy, cz};

    if (options.notifyTargetBoundaryChange) {
        tracker.notifyBoundaryChange(coord);
    } else if (options.targetState.has_value()) {
        tracker.setState(coord, *options.targetState);
    }

    if (options.activateAllSubRegions) {
        for (int lz = 0; lz < K_CHUNK_SIZE; lz += K_PHYS_TILE_SIZE) {
            for (int ly = 0; ly < K_CHUNK_SIZE; ly += K_PHYS_TILE_SIZE) {
                for (int lx = 0; lx < K_CHUNK_SIZE; lx += K_PHYS_TILE_SIZE)
                    tracker.markSubRegionActive(coord, lx, ly, lz);
            }
        }
    }

    notifyNeighbors(tracker, grid, cx, cy, cz, options.neighborInvalidation);
}

} // namespace recurse::simulation
