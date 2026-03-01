#pragma once

#include <array>
#include <cstdint>
#include <random>
#include <string>
#include <utility>
#include <vector>

namespace fabric {

/// Side length (in voxels) of a single WFC tile volume.
constexpr int kWFCTileSize = 4;

/// Total number of voxels in a single tile (kWFCTileSize^3).
constexpr int kWFCTileVolume = kWFCTileSize * kWFCTileSize * kWFCTileSize;

// ---------- Face indexing ----------

/// Face order: +X, -X, +Y, -Y, +Z, -Z (matches ChunkedGrid neighbor convention).
enum WFCFace : int {
    PosX = 0,
    NegX = 1,
    PosY = 2,
    NegY = 3,
    PosZ = 4,
    NegZ = 5,
};

/// Return the opposite face index.
constexpr int wfcOppositeFace(int face) {
    // +X <-> -X, +Y <-> -Y, +Z <-> -Z  (XOR with 1 flips the low bit).
    return face ^ 1;
}

/// 3D neighbor offset table indexed by WFCFace.
inline constexpr std::array<std::array<int, 3>, 6> kWFCNeighborOffsets = {{
    {{1, 0, 0}},  // PosX
    {{-1, 0, 0}}, // NegX
    {{0, 1, 0}},  // PosY
    {{0, -1, 0}}, // NegY
    {{0, 0, 1}},  // PosZ
    {{0, 0, -1}}, // NegZ
}};

// ---------- Tile ----------

/// A tile type that can be placed in the WFC grid.
struct WFCTile {
    int index = 0;                   ///< Unique tile index (0 is conventionally "air").
    float weight = 1.0f;             ///< Selection weight during collapse.
    std::array<int, 6> sockets = {}; ///< Socket ID per face (+X,-X,+Y,-Y,+Z,-Z).
};

// ---------- Adjacency ----------

/// Pre-computed per-face compatibility lists derived from socket matching.
/// adjacency[face][tileIndex] = set of tile indices compatible on that face.
struct WFCAdjacency {
    /// adjacency[face][tileIdx] -> list of compatible tile indices on that face.
    std::array<std::vector<std::vector<int>>, 6> compatible;

    /// Build adjacency from a tile palette.
    /// Two tiles are compatible on face F if tile1.sockets[F] == tile2.sockets[opposite(F)].
    static WFCAdjacency build(const std::vector<WFCTile>& tiles);
};

// ---------- Cell ----------

/// A single cell in the WFC grid.
struct WFCCell {
    std::vector<bool> possible; ///< Bitset of remaining tile possibilities.
    float entropy = 0.0f;       ///< Cached Shannon entropy.
    int collapsedIndex = -1;    ///< Tile index if collapsed, -1 otherwise.

    /// Number of remaining possibilities.
    int possibilityCount() const;

    /// Recompute Shannon entropy from current possibilities and tile weights.
    void updateEntropy(const std::vector<WFCTile>& tiles);

    /// Check whether this cell has been collapsed to a single tile.
    bool isCollapsed() const { return collapsedIndex >= 0; }
};

// ---------- Solve result ----------

/// Outcome of the WFC solve pass.
enum class WFCResult {
    Success,       ///< All cells collapsed without contradiction.
    Contradiction, ///< At least one cell hit a contradiction (resolved with air).
};

// ---------- Grid ----------

/// 3D grid of WFC cells (flat vector, row-major: x + y*width + z*width*height).
class WFCGrid {
  public:
    WFCGrid() = default;

    /// Initialize grid dimensions and set every cell to "all tiles possible".
    void init(int width, int height, int depth, const std::vector<WFCTile>& tiles);

    /// Access a cell by coordinate.
    WFCCell& cellAt(int x, int y, int z);
    const WFCCell& cellAt(int x, int y, int z) const;

    /// Find the uncollapsed cell with lowest Shannon entropy.
    /// Returns {x, y, z}. Ties are broken randomly using the provided RNG.
    /// Precondition: grid is not fully collapsed.
    std::array<int, 3> lowestEntropyCell(std::mt19937& rng) const;

    /// True when every cell has exactly one remaining possibility.
    bool isFullyCollapsed() const;

    int width() const { return width_; }
    int height() const { return height_; }
    int depth() const { return depth_; }

  private:
    int width_ = 0;
    int height_ = 0;
    int depth_ = 0;
    std::vector<WFCCell> cells_;

    int flatIndex(int x, int y, int z) const { return x + y * width_ + z * width_ * height_; }
};

// ---------- Solver free functions ----------

/// Collapse a cell to a single tile via weighted random selection.
void wfcCollapse(WFCCell& cell, const std::vector<WFCTile>& tiles, std::mt19937& rng);

/// BFS arc-consistency propagation from a starting cell.
/// Returns false if a contradiction was encountered (and resolved with air).
bool wfcPropagate(WFCGrid& grid, int startX, int startY, int startZ, const std::vector<WFCTile>& tiles,
                  const WFCAdjacency& adj);

/// Run the full WFC solve loop: lowest entropy -> collapse -> propagate -> repeat.
WFCResult wfcSolve(WFCGrid& grid, const std::vector<WFCTile>& tiles, uint32_t seed);

// ---------- Tile data (voxel content) ----------

/// Extended tile data carrying per-voxel density and essence plus socket and weight metadata.
struct WFCTileData {
    std::array<float, kWFCTileVolume> density{}; ///< Per-voxel density [0,1].
    std::array<float, kWFCTileVolume> essence{}; ///< Per-voxel essence ID.
    std::array<int, 6> sockets{};                ///< Socket ID per face (+X,-X,+Y,-Y,+Z,-Z).
    float weight = 1.0f;                         ///< Selection weight during collapse.
    std::string name;                            ///< Human-readable tile name.
};

/// An adjacency pair: indices of two tiles that can sit next to each other on a given face.
using WFCAdjPair = std::pair<int, int>;

/// A complete tile set: tiles + derived adjacency information.
struct WFCTileSet {
    std::vector<WFCTileData> tiles;         ///< All tile definitions.
    std::vector<WFCAdjPair> adjacencyPairs; ///< Explicit adjacency overrides (optional).

    /// Derive adjacency pairs from socket symmetry: +X face matches -X face, etc.
    /// Populates adjacencyPairs by checking all tile combinations.
    void deriveAdjacency();
};

// ---------- Tile set factories ----------

/// Create a dungeon-themed tile set with >= 5 tiles (air, corridor, room, wall, pillar, etc.).
WFCTileSet createDungeonTileSet();

/// Create a building-themed tile set with >= 5 tiles (air, floor, wall, window, roof, etc.).
WFCTileSet createBuildingTileSet();

} // namespace fabric
