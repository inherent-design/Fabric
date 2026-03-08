#pragma once
#include "recurse/simulation/VoxelMaterial.hh"
#include "fabric/world/ChunkedGrid.hh"
#include <array>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

namespace recurse::simulation {

struct ChunkBufferPair {
    using Buffer = std::array<VoxelCell, K_CHUNK_VOLUME>;
    std::unique_ptr<Buffer> buffers[2]; // nullptr = homogeneous sentinel
    VoxelCell fillValue{};
    void materialize();
    bool isMaterialized() const;
};

class SimulationGrid {
  public:
    SimulationGrid();
    VoxelCell readCell(int wx, int wy, int wz) const;
    VoxelCell readFromWriteBuffer(int wx, int wy, int wz) const;
    void writeCell(int wx, int wy, int wz, VoxelCell cell);
    void writeCellImmediate(int wx, int wy, int wz, VoxelCell cell);
    void advanceEpoch();
    uint64_t currentEpoch() const;
    void fillChunk(int cx, int cy, int cz, VoxelCell fill);
    void materializeChunk(int cx, int cy, int cz);
    bool isChunkMaterialized(int cx, int cy, int cz) const;
    size_t materializedChunkCount() const;
    const std::array<VoxelCell, K_CHUNK_VOLUME>* readBuffer(int cx, int cy, int cz) const;
    std::array<VoxelCell, K_CHUNK_VOLUME>* writeBuffer(int cx, int cy, int cz);
    bool hasChunk(int cx, int cy, int cz) const;
    VoxelCell getChunkFillValue(int cx, int cy, int cz) const;
    void removeChunk(int cx, int cy, int cz);
    std::vector<std::tuple<int, int, int>> allChunks() const;
    size_t chunkCount() const;
    void clear(); // Remove all chunks (for world reset)

  private:
    uint64_t epoch_ = 0;
    std::unordered_map<uint64_t, ChunkBufferPair> chunks_;
    VoxelCell readFromBuffer(int wx, int wy, int wz, int bufferIdx) const;
    int readIndex() const;
    int writeIndex() const;
};

} // namespace recurse::simulation
