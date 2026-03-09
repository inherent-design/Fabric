#pragma once
#include "fabric/world/ChunkedGrid.hh"
#include "recurse/simulation/VoxelMaterial.hh"
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

struct ChunkBufferPair {
    using Buffer = std::array<VoxelCell, K_CHUNK_VOLUME>;
    std::unique_ptr<Buffer> buffers[2]; // nullptr = homogeneous sentinel
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
    ChunkSlotState state = ChunkSlotState::Active;
    ChunkBufferPair simBuffers;

    // Pre-resolved raw pointers for zero-overhead worker access (wired by C-1c).
    VoxelCell* writePtr = nullptr;
    const VoxelCell* readPtr = nullptr;

    void materialize() { simBuffers.materialize(); }
    bool isMaterialized() const { return simBuffers.isMaterialized(); }
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
