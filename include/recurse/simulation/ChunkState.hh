#pragma once

#include "fabric/world/ChunkCoord.hh"
#include "recurse/simulation/ChunkRegistry.hh"
#include <optional>
#include <type_traits>

namespace recurse::simulation {

// ---------------------------------------------------------------------------
// Tag types for phantom state encoding (zero runtime overhead)
// ---------------------------------------------------------------------------

/// Slot exists in map but not yet allocated for use.
struct Absent {};

/// Terrain generation or async load in progress.
struct Generating {};

/// Fully loaded, available for simulation/rendering.
struct Active {};

/// Marked for removal, pending cleanup.
struct Draining {};

// ---------------------------------------------------------------------------
// Tag-to-enum mapping
// ---------------------------------------------------------------------------

/// Map a phantom state tag to the corresponding runtime ChunkSlotState value.
template <typename Tag> constexpr ChunkSlotState stateFor() {
    if constexpr (std::is_same_v<Tag, Absent>)
        return ChunkSlotState::Absent;
    else if constexpr (std::is_same_v<Tag, Generating>)
        return ChunkSlotState::Generating;
    else if constexpr (std::is_same_v<Tag, Active>)
        return ChunkSlotState::Active;
    else if constexpr (std::is_same_v<Tag, Draining>)
        return ChunkSlotState::Draining;
    else
        static_assert(!std::is_same_v<Tag, Tag>, "Unknown chunk state tag");
}

// ---------------------------------------------------------------------------
// Compile-time transition validation
// ---------------------------------------------------------------------------

/// Primary template: no valid transitions by default.
template <typename From, typename To> struct IsValidTransition : std::false_type {};

template <> struct IsValidTransition<Absent, Generating> : std::true_type {};
template <> struct IsValidTransition<Generating, Active> : std::true_type {};
template <> struct IsValidTransition<Active, Draining> : std::true_type {};

template <typename From, typename To>
concept ValidTransition = IsValidTransition<From, To>::value;

// ---------------------------------------------------------------------------
// ChunkRef<State>: phantom-typed handle to a chunk slot
// ---------------------------------------------------------------------------

/// Phantom-typed handle to a chunk slot.
/// Wraps a ChunkCoord and a non-owning pointer to the ChunkSlot.
/// State parameter constrains available API at compile time.
/// 24 bytes: ChunkCoord (12) + alignment padding (4) + ChunkSlot* (8).
template <typename State> class ChunkRef {
  public:
    ChunkRef(fabric::ChunkCoord coord, ChunkSlot* slot) : coord_(coord), slot_(slot) {}

    fabric::ChunkCoord coord() const { return coord_; }
    int cx() const { return coord_.x; }
    int cy() const { return coord_.y; }
    int cz() const { return coord_.z; }

    ChunkSlotState runtimeState() const { return slot_->state; }
    bool isMaterialized() const { return slot_->isMaterialized(); }

    // -- Absent/Generating: materialize --

    void materialize()
        requires std::is_same_v<State, Absent> || std::is_same_v<State, Generating>
    {
        slot_->materialize();
    }

    // -- Generating: write buffer + palette write --

    auto* writeBuffer(int bufIdx)
        requires std::is_same_v<State, Generating>
    {
        return slot_->simBuffers.buffers[bufIdx].get();
    }

    EssencePalette& palette()
        requires std::is_same_v<State, Generating> || std::is_same_v<State, Active>
    {
        return slot_->palette;
    }

    const EssencePalette& palette() const
        requires std::is_same_v<State, Active>
    {
        return slot_->palette;
    }

    // -- Active: full read/write --

    const VoxelCell* readPtr() const
        requires std::is_same_v<State, Active>
    {
        return slot_->readPtr;
    }

    VoxelCell* writePtr()
        requires std::is_same_v<State, Active>
    {
        return slot_->writePtr;
    }

    const auto* readBuffer(int bufIdx) const
        requires std::is_same_v<State, Active>
    {
        return slot_->simBuffers.buffers[bufIdx].get();
    }

    auto* writeBuffer(int bufIdx)
        requires std::is_same_v<State, Active>
    {
        return slot_->simBuffers.buffers[bufIdx].get();
    }

    void markDirty()
        requires std::is_same_v<State, Active>
    {
        slot_->copyCountdown = ChunkBuffers::K_COUNT - 1;
    }

    // -- Draining: coord-only (no additional API) --

  private:
    fabric::ChunkCoord coord_;
    ChunkSlot* slot_;

    template <typename From, typename To>
        requires ValidTransition<From, To>
    friend ChunkRef<To> transition(ChunkRef<From>, ChunkRegistry&);

    friend void cancelAndRemove(ChunkRef<Generating>, ChunkRegistry&);
};

// ChunkRef<T> layout: ChunkCoord (12 bytes) + padding (4 bytes) + ChunkSlot* (8 bytes) = 24 bytes.
// No virtual dispatch, no heap allocation.
static_assert(sizeof(ChunkRef<Active>) == 24);

// Compile-time verification: valid transitions
static_assert(ValidTransition<Absent, Generating>);
static_assert(ValidTransition<Generating, Active>);
static_assert(ValidTransition<Active, Draining>);

// Compile-time verification: invalid transitions do not satisfy the concept
static_assert(!ValidTransition<Absent, Active>);
static_assert(!ValidTransition<Absent, Draining>);
static_assert(!ValidTransition<Generating, Draining>);
static_assert(!ValidTransition<Active, Generating>);
static_assert(!ValidTransition<Draining, Active>);
static_assert(!ValidTransition<Draining, Generating>);
static_assert(!ValidTransition<Draining, Absent>);

// ---------------------------------------------------------------------------
// State transition function
// ---------------------------------------------------------------------------

/// Compile-time guarded state transition. Invalid transitions are compile errors.
/// Updates the runtime ChunkSlotState and returns a new ChunkRef with the target type.
template <typename From, typename To>
    requires ValidTransition<From, To>
ChunkRef<To> transition(ChunkRef<From> ref, ChunkRegistry& registry) {
    (void)registry;
    ref.slot_->state = stateFor<To>();
    return ChunkRef<To>{ref.coord(), ref.slot_};
}

/// Remove a generating chunk (cancellation/load failure path).
/// Distinct from active removal which goes through Draining.
inline void cancelAndRemove(ChunkRef<Generating> ref, ChunkRegistry& registry) {
    registry.removeChunk(ref.cx(), ref.cy(), ref.cz());
}

// ---------------------------------------------------------------------------
// Registry query helpers (free functions to avoid circular dependency)
// ---------------------------------------------------------------------------

/// Find a chunk slot and verify its runtime state matches the phantom type.
/// Returns nullopt if the slot does not exist or is in a different state.
template <typename State> std::optional<ChunkRef<State>> findAs(ChunkRegistry& registry, int cx, int cy, int cz) {
    auto* slot = registry.find(cx, cy, cz);
    if (!slot || slot->state != stateFor<State>())
        return std::nullopt;
    return ChunkRef<State>{fabric::ChunkCoord{cx, cy, cz}, slot};
}

/// Add a new chunk to the registry and return a typed Absent handle.
/// If the chunk already exists (idempotent addChunk), the slot is returned as-is.
inline ChunkRef<Absent> addChunkRef(ChunkRegistry& registry, int cx, int cy, int cz) {
    auto& slot = registry.addChunk(cx, cy, cz);
    return ChunkRef<Absent>{fabric::ChunkCoord{cx, cy, cz}, &slot};
}

} // namespace recurse::simulation
