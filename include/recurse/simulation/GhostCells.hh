#pragma once
#include "fabric/world/ChunkedGrid.hh"
#include "recurse/simulation/ChunkActivityTracker.hh"
#include "recurse/simulation/SimulationGrid.hh"
#include "recurse/simulation/VoxelMaterial.hh"
#include <array>
#include <unordered_map>
#include <vector>

namespace recurse::simulation {

enum class Face : uint8_t {
    PosX = 0,
    NegX,
    PosY,
    NegY,
    PosZ,
    NegZ
};
inline constexpr int K_FACE_COUNT = 6;
inline constexpr int K_FACE_AREA = K_CHUNK_SIZE * K_CHUNK_SIZE; // 1024

struct GhostCellStore {
    std::array<std::array<VoxelCell, K_FACE_AREA>, K_FACE_COUNT> faces{};

    VoxelCell get(Face face, int u, int v) const;
    void set(Face face, int u, int v, VoxelCell cell);
};

class GhostCellManager {
  public:
    void syncGhostCells(ChunkPos pos, const SimulationGrid& grid);
    void syncAll(const std::vector<ChunkPos>& chunks, const SimulationGrid& grid);
    VoxelCell readGhost(ChunkPos pos, int lx, int ly, int lz) const;
    GhostCellStore& getStore(ChunkPos pos);
    void remove(ChunkPos pos);

  private:
    std::unordered_map<ChunkPos, GhostCellStore, ChunkPosHash> stores_;
};

} // namespace recurse::simulation
