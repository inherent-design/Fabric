#pragma once

#include "recurse/simulation/ChunkActivityTracker.hh"

#include <optional>
#include <span>

namespace recurse::simulation {

class MaterialRegistry;
class SimulationGrid;

enum class NeighborInvalidation : uint8_t {
    None,
    Face,
    FaceAndDiagonalXZ,
};

struct ChunkBufferFinalizationOptions {
    int sourceBufferIndex = -1;
    const MaterialRegistry* materials = nullptr;
    std::span<const float> paletteData{};
    bool restorePalette = false;
};

struct ChunkActivationOptions {
    std::optional<ChunkState> targetState = std::nullopt;
    bool activateAllSubRegions = false;
    bool notifyTargetBoundaryChange = false;
    NeighborInvalidation neighborInvalidation = NeighborInvalidation::None;
};

void finalizeChunkBuffers(SimulationGrid& grid, int cx, int cy, int cz,
                          const ChunkBufferFinalizationOptions& options = {});

void finalizeChunkActivation(ChunkActivityTracker& tracker, const SimulationGrid& grid, int cx, int cy, int cz,
                             const ChunkActivationOptions& options = {});

} // namespace recurse::simulation
