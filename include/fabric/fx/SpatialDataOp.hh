#pragma once

#include "fabric/fx/Never.hh"
#include "fabric/fx/OneOf.hh"
#include "fabric/world/ChunkCoord.hh"

#include <utility>
#include <vector>

namespace fabric::fx {

/// Low-level world-space read of a raw stored value.
///
/// These spatial ops intentionally describe storage-facing access. Higher-level
/// semantic or mesh-facing queries should layer on top through WorldContext
/// instead of baking rendering or gameplay meaning into this contract.
template <typename T> struct SpatialRead {
    int wx, wy, wz;

    static constexpr bool K_IS_SYNC = true;
    using Returns = T;
    using Errors = TypeList<Never>;
};

/// Low-level world-space write of a raw stored value.
template <typename T> struct SpatialWrite {
    int wx, wy, wz;
    T value;

    static constexpr bool K_IS_SYNC = false;
    using Returns = void;
    using Errors = TypeList<Never>;
};

/// Low-level swap of two raw stored values.
struct SpatialSwap {
    int ax, ay, az;
    int bx, by, bz;

    static constexpr bool K_IS_SYNC = false;
    using Returns = void;
    using Errors = TypeList<Never>;
};

/// Query a region for raw stored values matching a predicate.
///
/// This remains a storage-level query surface during the current Goal #4 plus
/// meshing rollout. Richer semantic or mesh-facing queries are expected to
/// compose above it instead of replacing the underlying op boundary.
template <typename T, typename Pred> struct SpatialQuery {
    ChunkCoord min;
    ChunkCoord max;
    Pred predicate;

    static constexpr bool K_IS_SYNC = true;
    using Returns = std::vector<std::pair<ChunkCoord, T>>;
    using Errors = TypeList<Never>;
};

} // namespace fabric::fx
