#pragma once

namespace recurse {

/// Per-frame voxel telemetry counters, written by simulation and meshing
/// systems. Game-specific; not part of the engine RuntimeState.
struct VoxelStats {
    int visibleChunks = 0;
    int totalChunks = 0;
    int meshQueueSize = 0;
};

} // namespace recurse
