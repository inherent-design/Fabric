#pragma once
#include "recurse/simulation/ChunkRegistry.hh"
#include <cstdint>
#include <tuple>
#include <vector>

namespace recurse::simulation {

class SimulationGrid {
  public:
    SimulationGrid();

    // Cell-level API (unchanged signatures)
    VoxelCell readCell(int wx, int wy, int wz) const;
    VoxelCell readFromWriteBuffer(int wx, int wy, int wz) const;
    void writeCell(int wx, int wy, int wz, VoxelCell cell);
    bool writeCellIfExists(int wx, int wy, int wz, VoxelCell cell);
    void writeCellImmediate(int wx, int wy, int wz, VoxelCell cell);
    void advanceEpoch();
    void syncChunkBuffers(int cx, int cy, int cz);
    uint64_t currentEpoch() const;

    // Chunk-level API (unchanged signatures, delegates to registry_)
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
    void clear();

    // Direct registry access (for systems needing structural control)
    ChunkRegistry& registry();
    const ChunkRegistry& registry() const;

  private:
    uint64_t epoch_ = 0;
    ChunkRegistry registry_;
    VoxelCell readFromBuffer(int wx, int wy, int wz, int bufferIdx) const;
    int readIndex() const;
    int writeIndex() const;
};

} // namespace recurse::simulation
