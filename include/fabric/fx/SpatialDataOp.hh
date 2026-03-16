#pragma once

#include "fabric/fx/Never.hh"
#include "fabric/fx/OneOf.hh"
#include "fabric/world/ChunkCoord.hh"

#include <utility>
#include <vector>

namespace fabric::fx {

/// Read a single value at a world-space position from a ChunkedGrid<T>.
template <typename T> struct SpatialRead {
    int wx, wy, wz;

    static constexpr bool K_IS_SYNC = true;
    using Returns = T;
    using Errors = TypeList<Never>;
};

/// Write a single value at a world-space position into a ChunkedGrid<T>.
template <typename T> struct SpatialWrite {
    int wx, wy, wz;
    T value;

    static constexpr bool K_IS_SYNC = false;
    using Returns = void;
    using Errors = TypeList<Never>;
};

/// Swap values at two world-space positions.
struct SpatialSwap {
    int ax, ay, az;
    int bx, by, bz;

    static constexpr bool K_IS_SYNC = false;
    using Returns = void;
    using Errors = TypeList<Never>;
};

/// Query a region with a predicate, returning matching positions and values.
template <typename T, typename Pred> struct SpatialQuery {
    ChunkCoord min;
    ChunkCoord max;
    Pred predicate;

    static constexpr bool K_IS_SYNC = true;
    using Returns = std::vector<std::pair<ChunkCoord, T>>;
    using Errors = TypeList<Never>;
};

} // namespace fabric::fx
