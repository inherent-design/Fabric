#pragma once
#include "recurse/simulation/ChunkActivityTracker.hh"
#include "recurse/simulation/VoxelMaterial.hh"
#include <vector>

namespace recurse::simulation {

struct BoundaryWrite {
    int dstWx, dstWy, dstWz;
    VoxelCell writeCell;
    int srcWx, srcWy, srcWz;
    VoxelCell undoCell;
    ChunkPos neighborChunk;
};

using BoundaryWriteQueue = std::vector<BoundaryWrite>;

} // namespace recurse::simulation
