#pragma once
#include "fabric/world/ChunkedGrid.hh"
#include "recurse/simulation/ChunkActivityTracker.hh"
#include "recurse/simulation/VoxelMaterial.hh"
#include "recurse/world/EssencePalette.hh"
#include <array>
#include <cstdint>
#include <memory>
#include <tuple>
#include <unordered_map>
#include <vector>

namespace recurse::simulation {

using fabric::K_CHUNK_SIZE;
using fabric::K_CHUNK_VOLUME;
using fabric::packChunkKey;
using fabric::unpackChunkKey;

struct ChunkBuffers {
    /// Triple-buffered: epoch advance copies only dirty chunks (D-26, D-42).
    /// K_COUNT=3 eliminates read/write contention; copyCountdown eliminates
    /// copies for stable chunks (~95% of materialized set).
    static constexpr int K_COUNT = 3;
    using Buffer = std::array<VoxelCell, K_CHUNK_VOLUME>;
    std::unique_ptr<Buffer> buffers[K_COUNT];
    VoxelCell fillValue{};
    void materialize();
    bool isMaterialized() const;
};

enum class ChunkSlotState : uint8_t {
    Absent,
    Generating,
    Active,
    Draining
};

struct ChunkSlot {
    ChunkSlotState state = ChunkSlotState::Absent;
    ChunkBuffers simBuffers;
    recurse::EssencePalette palette{recurse::EssencePalette::K_DEFAULT_EPSILON, 256};

    // Pre-resolved raw pointers for zero-overhead worker access (wired by C-1c).
    VoxelCell* writePtr = nullptr;
    const VoxelCell* readPtr = nullptr;

    /// Dirty tracking for advanceEpoch. When a chunk is written (simulation,
    /// boundary drain, voxel interaction), set to K_COUNT-1. advanceEpoch
    /// copies write->next_write_target and decrements. After K_COUNT-1 clean
    /// epochs, all buffers converge and no further copies occur.
    uint8_t copyCountdown{0};

    void materialize() { simBuffers.materialize(); }
    bool isMaterialized() const { return simBuffers.isMaterialized(); }
};

struct ChunkDispatchEntry {
    ChunkCoord pos;
    ChunkSlot* slot;
};

class ChunkRegistry {
  public:
    // Structural modification
    ChunkSlot& addChunk(int cx, int cy, int cz);
    void removeChunk(int cx, int cy, int cz);
    void clear();

    // Lookup (read-only on map topology)
    ChunkSlot* find(int cx, int cy, int cz);
    const ChunkSlot* find(int cx, int cy, int cz) const;
    bool hasChunk(int cx, int cy, int cz) const;

    // Bulk queries
    size_t chunkCount() const;
    size_t materializedChunkCount() const;
    std::vector<std::tuple<int, int, int>> allChunks() const;

    // Dispatch helpers (C-1c)
    std::vector<ChunkDispatchEntry> buildDispatchList(ChunkSlotState filter);
    void resolveBufferPointers(uint64_t epoch);

    // Iteration over materialized chunks (used by advanceEpoch)
    template <typename Fn> void forEachMaterialized(Fn&& fn) {
        for (auto& [key, slot] : slots_) {
            if (slot.isMaterialized())
                fn(slot);
        }
    }

  private:
    std::unordered_map<uint64_t, ChunkSlot> slots_;
};

} // namespace recurse::simulation
