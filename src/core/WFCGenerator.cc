#include "fabric/core/WFCGenerator.hh"

#include <cmath>
#include <deque>
#include <limits>

namespace fabric {

// ---------- WFCAdjacency ----------

WFCAdjacency WFCAdjacency::build(const std::vector<WFCTile>& tiles) {
    WFCAdjacency adj;
    int n = static_cast<int>(tiles.size());

    for (int face = 0; face < 6; ++face) {
        adj.compatible[face].resize(n);
        int opp = wfcOppositeFace(face);

        for (int a = 0; a < n; ++a) {
            for (int b = 0; b < n; ++b) {
                if (tiles[a].sockets[face] == tiles[b].sockets[opp]) {
                    adj.compatible[face][a].push_back(b);
                }
            }
        }
    }

    return adj;
}

// ---------- WFCCell ----------

int WFCCell::possibilityCount() const {
    int count = 0;
    for (bool b : possible) {
        if (b)
            ++count;
    }
    return count;
}

void WFCCell::updateEntropy(const std::vector<WFCTile>& tiles) {
    // Shannon entropy: H = -sum(p_i * log(p_i)) where p_i = w_i / totalWeight
    // among remaining tiles.
    float totalWeight = 0.0f;
    for (int i = 0; i < static_cast<int>(possible.size()); ++i) {
        if (possible[i])
            totalWeight += tiles[i].weight;
    }

    if (totalWeight <= 0.0f) {
        entropy = 0.0f;
        return;
    }

    float h = 0.0f;
    for (int i = 0; i < static_cast<int>(possible.size()); ++i) {
        if (possible[i]) {
            float p = tiles[i].weight / totalWeight;
            if (p > 0.0f)
                h -= p * std::log(p);
        }
    }

    entropy = h;
}

// ---------- WFCGrid ----------

void WFCGrid::init(int width, int height, int depth, const std::vector<WFCTile>& tiles) {
    width_ = width;
    height_ = height;
    depth_ = depth;

    int total = width * height * depth;
    int tileCount = static_cast<int>(tiles.size());

    cells_.resize(total);
    for (auto& cell : cells_) {
        cell.possible.assign(tileCount, true);
        cell.collapsedIndex = -1;
        cell.updateEntropy(tiles);
    }
}

WFCCell& WFCGrid::cellAt(int x, int y, int z) {
    return cells_[flatIndex(x, y, z)];
}

const WFCCell& WFCGrid::cellAt(int x, int y, int z) const {
    return cells_[flatIndex(x, y, z)];
}

std::array<int, 3> WFCGrid::lowestEntropyCell(std::mt19937& rng) const {
    float minEntropy = std::numeric_limits<float>::max();
    std::vector<std::array<int, 3>> candidates;

    for (int z = 0; z < depth_; ++z) {
        for (int y = 0; y < height_; ++y) {
            for (int x = 0; x < width_; ++x) {
                const auto& cell = cells_[flatIndex(x, y, z)];
                if (cell.isCollapsed())
                    continue;

                int count = cell.possibilityCount();
                if (count <= 1)
                    continue; // skip already-determined or empty cells

                if (cell.entropy < minEntropy - 1e-6f) {
                    minEntropy = cell.entropy;
                    candidates.clear();
                    candidates.push_back({x, y, z});
                } else if (std::abs(cell.entropy - minEntropy) < 1e-6f) {
                    candidates.push_back({x, y, z});
                }
            }
        }
    }

    if (candidates.empty()) {
        // Fallback: return first uncollapsed cell (shouldn't happen in normal flow).
        for (int z = 0; z < depth_; ++z) {
            for (int y = 0; y < height_; ++y) {
                for (int x = 0; x < width_; ++x) {
                    if (!cells_[flatIndex(x, y, z)].isCollapsed())
                        return {x, y, z};
                }
            }
        }
        return {0, 0, 0};
    }

    // Randomize among ties.
    std::uniform_int_distribution<int> dist(0, static_cast<int>(candidates.size()) - 1);
    return candidates[dist(rng)];
}

bool WFCGrid::isFullyCollapsed() const {
    for (const auto& cell : cells_) {
        if (!cell.isCollapsed())
            return false;
    }
    return true;
}

// ---------- Collapse ----------

void wfcCollapse(WFCCell& cell, const std::vector<WFCTile>& tiles, std::mt19937& rng) {
    // Weighted random selection among remaining possibilities.
    float totalWeight = 0.0f;
    for (int i = 0; i < static_cast<int>(cell.possible.size()); ++i) {
        if (cell.possible[i])
            totalWeight += tiles[i].weight;
    }

    if (totalWeight <= 0.0f) {
        // No possibilities: contradiction — collapse to air (index 0).
        cell.possible.assign(cell.possible.size(), false);
        if (!cell.possible.empty())
            cell.possible[0] = true;
        cell.collapsedIndex = 0;
        cell.entropy = 0.0f;
        return;
    }

    std::uniform_real_distribution<float> dist(0.0f, totalWeight);
    float roll = dist(rng);

    float cumulative = 0.0f;
    int chosen = -1;
    for (int i = 0; i < static_cast<int>(cell.possible.size()); ++i) {
        if (cell.possible[i]) {
            cumulative += tiles[i].weight;
            if (roll <= cumulative) {
                chosen = i;
                break;
            }
        }
    }

    // Edge case: floating point rounding — pick last possible tile.
    if (chosen < 0) {
        for (int i = static_cast<int>(cell.possible.size()) - 1; i >= 0; --i) {
            if (cell.possible[i]) {
                chosen = i;
                break;
            }
        }
    }

    // Set cell to collapsed state.
    cell.possible.assign(cell.possible.size(), false);
    cell.possible[chosen] = true;
    cell.collapsedIndex = chosen;
    cell.entropy = 0.0f;
}

// ---------- Propagation ----------

bool wfcPropagate(WFCGrid& grid, int startX, int startY, int startZ, const std::vector<WFCTile>& tiles,
                  const WFCAdjacency& adj) {
    bool contradiction = false;

    // BFS queue of cell coordinates that changed.
    std::deque<std::array<int, 3>> queue;
    queue.push_back({startX, startY, startZ});

    while (!queue.empty()) {
        auto [cx, cy, cz] = queue.front();
        queue.pop_front();

        // For each face/neighbor direction...
        for (int face = 0; face < 6; ++face) {
            int nx = cx + kWFCNeighborOffsets[face][0];
            int ny = cy + kWFCNeighborOffsets[face][1];
            int nz = cz + kWFCNeighborOffsets[face][2];

            if (nx < 0 || nx >= grid.width() || ny < 0 || ny >= grid.height() || nz < 0 || nz >= grid.depth())
                continue;

            auto& neighbor = grid.cellAt(nx, ny, nz);
            if (neighbor.isCollapsed())
                continue;

            const auto& source = grid.cellAt(cx, cy, cz);

            // Build the set of tiles that are compatible with ANY remaining
            // tile in the source cell on this face.
            int tileCount = static_cast<int>(tiles.size());
            std::vector<bool> allowed(tileCount, false);

            for (int srcTile = 0; srcTile < tileCount; ++srcTile) {
                if (!source.possible[srcTile])
                    continue;
                for (int compatTile : adj.compatible[face][srcTile]) {
                    allowed[compatTile] = true;
                }
            }

            // Remove any neighbor possibility not in the allowed set.
            bool changed = false;
            for (int t = 0; t < tileCount; ++t) {
                if (neighbor.possible[t] && !allowed[t]) {
                    neighbor.possible[t] = false;
                    changed = true;
                }
            }

            if (changed) {
                int remaining = neighbor.possibilityCount();
                if (remaining == 0) {
                    // Contradiction: resolve with air (index 0).
                    contradiction = true;
                    neighbor.possible.assign(tileCount, false);
                    if (tileCount > 0)
                        neighbor.possible[0] = true;
                    neighbor.collapsedIndex = 0;
                    neighbor.entropy = 0.0f;
                } else if (remaining == 1) {
                    // Auto-collapse: find the single remaining tile.
                    for (int t = 0; t < tileCount; ++t) {
                        if (neighbor.possible[t]) {
                            neighbor.collapsedIndex = t;
                            break;
                        }
                    }
                    neighbor.entropy = 0.0f;
                    queue.push_back({nx, ny, nz});
                } else {
                    neighbor.updateEntropy(tiles);
                    queue.push_back({nx, ny, nz});
                }
            }
        }
    }

    return !contradiction;
}

// ---------- Solve ----------

WFCResult wfcSolve(WFCGrid& grid, const std::vector<WFCTile>& tiles, uint32_t seed) {
    std::mt19937 rng(seed);
    WFCAdjacency adj = WFCAdjacency::build(tiles);
    bool hadContradiction = false;

    while (!grid.isFullyCollapsed()) {
        auto [x, y, z] = grid.lowestEntropyCell(rng);
        auto& cell = grid.cellAt(x, y, z);

        wfcCollapse(cell, tiles, rng);

        if (!wfcPropagate(grid, x, y, z, tiles, adj)) {
            hadContradiction = true;
        }
    }

    return hadContradiction ? WFCResult::Contradiction : WFCResult::Success;
}

// ---------- WFCTileSet::deriveAdjacency ----------

void WFCTileSet::deriveAdjacency() {
    adjacencyPairs.clear();
    int n = static_cast<int>(tiles.size());

    for (int face = 0; face < 6; ++face) {
        int opp = wfcOppositeFace(face);
        for (int a = 0; a < n; ++a) {
            for (int b = 0; b < n; ++b) {
                if (tiles[a].sockets[face] == tiles[b].sockets[opp]) {
                    // Encode: face * n*n + a * n + b (unique per triple).
                    adjacencyPairs.push_back({a, b});
                }
            }
        }
    }
}

// ---------- Factory helpers ----------

namespace {

/// Helper: create a WFCTileData with uniform density and essence, named sockets, and weight.
WFCTileData makeTileData(const std::string& name, float uniformDensity, float uniformEssence,
                         const std::array<int, 6>& sockets, float weight = 1.0f) {
    WFCTileData tile;
    tile.name = name;
    tile.weight = weight;
    tile.sockets = sockets;
    tile.density.fill(uniformDensity);
    tile.essence.fill(uniformEssence);
    return tile;
}

} // anonymous namespace

// ---------- createDungeonTileSet ----------

WFCTileSet createDungeonTileSet() {
    WFCTileSet ts;

    // Socket convention:
    //   0 = air-air boundary
    //   1 = wall face (solid exterior)
    //   2 = corridor opening
    //   3 = pillar contact

    // Tile 0: Air (empty space)
    ts.tiles.push_back(makeTileData("air", 0.0f, 0.0f, {0, 0, 0, 0, 0, 0}, 3.0f));

    // Tile 1: Corridor (open on +X/-X, walls on Y/Z)
    ts.tiles.push_back(makeTileData("corridor", 0.2f, 1.0f, {2, 2, 1, 1, 1, 1}, 2.0f));

    // Tile 2: Room (open on all horizontal faces, floor/ceiling on Y)
    ts.tiles.push_back(makeTileData("room", 0.1f, 2.0f, {2, 2, 1, 1, 2, 2}, 1.0f));

    // Tile 3: Wall (solid on all faces)
    ts.tiles.push_back(makeTileData("wall", 1.0f, 3.0f, {1, 1, 1, 1, 1, 1}, 2.0f));

    // Tile 4: Pillar (solid, but with pillar sockets on Y)
    ts.tiles.push_back(makeTileData("pillar", 0.9f, 4.0f, {1, 1, 3, 3, 1, 1}, 0.5f));

    // Tile 5: Door (open on +X/-X, solid elsewhere, transition piece)
    ts.tiles.push_back(makeTileData("door", 0.3f, 5.0f, {2, 2, 1, 1, 1, 1}, 0.5f));

    ts.deriveAdjacency();
    return ts;
}

// ---------- createBuildingTileSet ----------

WFCTileSet createBuildingTileSet() {
    WFCTileSet ts;

    // Socket convention:
    //   0 = air
    //   10 = wall exterior
    //   11 = wall interior
    //   12 = window opening
    //   13 = floor/ceiling contact

    // Tile 0: Air
    ts.tiles.push_back(makeTileData("air", 0.0f, 0.0f, {0, 0, 0, 0, 0, 0}, 3.0f));

    // Tile 1: Floor slab (solid on +Y/-Y, open on horizontal)
    ts.tiles.push_back(makeTileData("floor", 0.8f, 10.0f, {11, 11, 13, 13, 11, 11}, 2.0f));

    // Tile 2: Wall section (solid on +X/-X, stacks on Y)
    ts.tiles.push_back(makeTileData("wall", 1.0f, 11.0f, {10, 10, 13, 13, 10, 10}, 2.0f));

    // Tile 3: Window (wall with opening on +Z/-Z)
    ts.tiles.push_back(makeTileData("window", 0.5f, 12.0f, {10, 10, 13, 13, 12, 12}, 1.0f));

    // Tile 4: Roof (solid on -Y, air on +Y)
    ts.tiles.push_back(makeTileData("roof", 0.9f, 13.0f, {10, 10, 0, 13, 10, 10}, 1.0f));

    // Tile 5: Interior (open space inside building)
    ts.tiles.push_back(makeTileData("interior", 0.0f, 14.0f, {11, 11, 13, 13, 11, 11}, 1.5f));

    ts.deriveAdjacency();
    return ts;
}

} // namespace fabric
