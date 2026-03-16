#pragma once

#include "fabric/fx/Error.hh"
#include "fabric/fx/Never.hh"
#include "fabric/fx/OneOf.hh"
#include "fabric/world/ChunkCoord.hh"

#include <tuple>
#include <vector>

namespace recurse::simulation {
struct VoxelCell;
struct ChunkSlot;
} // namespace recurse::simulation

namespace recurse {
class WorldSession;
} // namespace recurse

namespace recurse::ops {

// --- Sync Read Operations ---

/// Check if a chunk exists in the simulation grid.
struct HasChunk {
    int cx, cy, cz;

    static constexpr bool K_IS_SYNC = true;
    using Returns = bool;
    using Errors = fabric::fx::TypeList<fabric::fx::Never>;
    // TODO: using RequiresState = ...; // Phase IV type-state
};

/// Find a chunk slot in the registry.
struct FindSlot {
    int cx, cy, cz;

    static constexpr bool K_IS_SYNC = true;
    using Returns = const simulation::ChunkSlot*;
    using Errors = fabric::fx::TypeList<fabric::fx::Never>;
    // TODO: using RequiresState = ...; // Phase IV type-state
};

/// Check if coordinates are within the DB's saved bounding region.
struct IsInSavedRegion {
    int cx, cy, cz;

    static constexpr bool K_IS_SYNC = true;
    using Returns = bool;
    using Errors = fabric::fx::TypeList<fabric::fx::Never>;
};

/// Check if a chunk has a pending async load in flight.
struct HasPendingLoad {
    int cx, cy, cz;

    static constexpr bool K_IS_SYNC = true;
    using Returns = bool;
    using Errors = fabric::fx::TypeList<fabric::fx::Never>;
};

/// Query the chunk entity map. Returns true if the chunk has an ECS entity.
struct QueryChunkEntities {
    fabric::ChunkCoord coord;

    static constexpr bool K_IS_SYNC = true;
    using Returns = bool;
    using Errors = fabric::fx::TypeList<fabric::fx::Never>;
};

/// Read the voxel buffer for a chunk at the given buffer index.
struct ReadBuffer {
    int cx, cy, cz;
    int bufIdx;

    static constexpr bool K_IS_SYNC = true;
    using Returns = const simulation::VoxelCell*;
    using Errors = fabric::fx::TypeList<fabric::fx::Never>;
    // TODO: using RequiresState = Active; // Phase IV type-state
};

/// Get a mutable pointer to the write buffer for a chunk.
struct WriteBuffer {
    int cx, cy, cz;
    int bufIdx;

    static constexpr bool K_IS_SYNC = true;
    using Returns = simulation::VoxelCell*;
    using Errors = fabric::fx::TypeList<fabric::fx::Never>;
    // TODO: using RequiresState = Active; // Phase IV type-state
};

/// Get the total number of chunks in the registry.
struct ChunkCount {
    static constexpr bool K_IS_SYNC = true;
    using Returns = int;
    using Errors = fabric::fx::TypeList<fabric::fx::Never>;
};

/// Get the number of active (simulated) chunks.
struct ActiveChunkCount {
    static constexpr bool K_IS_SYNC = true;
    using Returns = int;
    using Errors = fabric::fx::TypeList<fabric::fx::Never>;
};

/// Describes a single completed async chunk load.
struct CompletedLoad {
    int cx, cy, cz;
    int bufferIndex;
    bool success;
};

/// Poll pending async load completions, returning up to maxCompletions results.
/// Caller is responsible for entity creation and system notifications.
struct PollPendingLoads {
    int maxCompletions;

    static constexpr bool K_IS_SYNC = true;
    using Returns = std::vector<CompletedLoad>;
    using Errors = fabric::fx::TypeList<fabric::fx::Never>;
};

/// Query the set of LOD chunks currently tracked.
struct QueryLODChunks {
    static constexpr bool K_IS_SYNC = true;
    using Returns = int;
    using Errors = fabric::fx::TypeList<fabric::fx::Never>;
};

// --- Async Mutation Operations ---

/// Dispatch an async chunk load from SQLite.
struct LoadChunk {
    int cx, cy, cz;

    static constexpr bool K_IS_SYNC = false;
    using Returns = void;
    using Errors = fabric::fx::TypeList<fabric::fx::IOError>;
    // TODO: using RequiresState = Absent; // Phase IV type-state
};

/// Encode and queue a chunk for saving.
struct SaveChunk {
    int cx, cy, cz;

    static constexpr bool K_IS_SYNC = false;
    using Returns = void;
    using Errors = fabric::fx::TypeList<fabric::fx::IOError>;
    // TODO: using RequiresState = Active; // Phase IV type-state
};

/// Remove a chunk from the simulation grid.
struct RemoveChunk {
    int cx, cy, cz;

    static constexpr bool K_IS_SYNC = false;
    using Returns = void;
    using Errors = fabric::fx::TypeList<fabric::fx::StateError>;
    // TODO: using RequiresState = Active; // Phase IV type-state
};

/// Cancel a pending async load.
struct CancelPendingLoad {
    int cx, cy, cz;

    static constexpr bool K_IS_SYNC = false;
    using Returns = bool;
    using Errors = fabric::fx::TypeList<fabric::fx::Never>;
};

/// Batch-generate chunks via parallelFor.
struct GenerateChunks {
    std::vector<std::tuple<int, int, int>> coords;

    static constexpr bool K_IS_SYNC = false;
    using Returns = void;
    using Errors = fabric::fx::TypeList<fabric::fx::Never>;
};

/// Per-frame tick combining flush, save service, snapshot, and pruning updates.
struct Tick {
    float dt;

    static constexpr bool K_IS_SYNC = false;
    using Returns = void;
    using Errors = fabric::fx::TypeList<fabric::fx::Never>;
};

/// Update the LOD ring around the player position.
struct UpdateLODRing {
    float px, py, pz;
    int chunkRadius;
    int lodRadius;
    int genBudget;

    static constexpr bool K_IS_SYNC = false;
    using Returns = void;
    using Errors = fabric::fx::TypeList<fabric::fx::Never>;
};

} // namespace recurse::ops
