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

class ChunkRegistry {
  public:
    // Structural modification
    ChunkBufferPair& addChunk(int cx, int cy, int cz);
    void removeChunk(int cx, int cy, int cz);
    void clear();

    // Lookup (read-only on map topology)
    ChunkBufferPair* find(int cx, int cy, int cz);
    const ChunkBufferPair* find(int cx, int cy, int cz) const;
    bool hasChunk(int cx, int cy, int cz) const;

    // Bulk queries
    size_t chunkCount() const;
    size_t materializedChunkCount() const;
    std::vector<std::tuple<int, int, int>> allChunks() const;

    // Iteration over materialized chunks (used by advanceEpoch)
    template <typename Fn> void forEachMaterialized(Fn&& fn) {
        for (auto& [key, pair] : slots_) {
            if (pair.isMaterialized())
                fn(pair);
        }
    }

  private:
    std::unordered_map<uint64_t, ChunkBufferPair> slots_;
};

} // namespace recurse::simulation
