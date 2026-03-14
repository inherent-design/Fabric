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
    void syncGhostCells(ChunkCoord pos, const SimulationGrid& grid);
    void syncAll(const std::vector<ChunkCoord>& chunks, const SimulationGrid& grid);
    VoxelCell readGhost(ChunkCoord pos, int lx, int ly, int lz) const;
    GhostCellStore& getStore(ChunkCoord pos);
    void remove(ChunkCoord pos);

    /// Remove all ghost cell stores. Called during world reset.
    void clear() { stores_.clear(); }

  private:
    std::unordered_map<ChunkCoord, GhostCellStore, ChunkCoordHash> stores_;
};

} // namespace recurse::simulation
