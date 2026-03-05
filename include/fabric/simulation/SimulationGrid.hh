#pragma once
#include "fabric/simulation/VoxelMaterial.hh"
#include "recurse/world/ChunkedGrid.hh"
#include <array>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

namespace fabric::simulation {

using recurse::kChunkSize;
using recurse::kChunkVolume;

struct ChunkBufferPair {
    using Buffer = std::array<VoxelCell, kChunkVolume>;
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
    void advanceEpoch();
    uint64_t currentEpoch() const;
    void fillChunk(int cx, int cy, int cz, VoxelCell fill);
    void materializeChunk(int cx, int cy, int cz);
    bool isChunkMaterialized(int cx, int cy, int cz) const;
    size_t materializedChunkCount() const;
    const std::array<VoxelCell, kChunkVolume>* readBuffer(int cx, int cy, int cz) const;
    std::array<VoxelCell, kChunkVolume>* writeBuffer(int cx, int cy, int cz);
    bool hasChunk(int cx, int cy, int cz) const;
    void removeChunk(int cx, int cy, int cz);
    std::vector<std::tuple<int, int, int>> allChunks() const;

  private:
    uint64_t epoch_ = 0;
    std::unordered_map<int64_t, ChunkBufferPair> chunks_;
    static int64_t packKey(int cx, int cy, int cz);
    int readIndex() const;
    int writeIndex() const;
};

} // namespace fabric::simulation
