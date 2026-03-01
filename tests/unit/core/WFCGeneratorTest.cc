#include "fabric/core/WFCGenerator.hh"

#include <gtest/gtest.h>

#include <cmath>
#include <set>

using namespace fabric;

// ---------------------------------------------------------------------------
// Helper: build a simple 2-tile palette for basic tests.
//
// Tile 0 ("air"):  socket 0 on all faces.
// Tile 1 ("solid"): socket 1 on all faces.
//
// Socket matching: tiles with the same socket on opposite faces are compatible.
// So air-air and solid-solid are compatible; air-solid is NOT.
// ---------------------------------------------------------------------------
static std::vector<WFCTile> makeTwoTilePalette() {
    WFCTile air;
    air.index = 0;
    air.weight = 1.0f;
    air.sockets = {0, 0, 0, 0, 0, 0};

    WFCTile solid;
    solid.index = 1;
    solid.weight = 1.0f;
    solid.sockets = {1, 1, 1, 1, 1, 1};

    return {air, solid};
}

// ---------------------------------------------------------------------------
// Helper: build a 3-tile palette where tiles can chain.
//
// Tile 0 ("air"):     sockets all 0.
// Tile 1 ("border"):  +X/+Y/+Z=1, -X/-Y/-Z=0  (one-way connector).
// Tile 2 ("core"):    sockets all 1.
//
// air  connects to air  on any face (0==0).
// air  connects to border on -X,-Y,-Z faces (border.-X=0 == air.+X=0).
// border connects to core on +X,+Y,+Z faces (border.+X=1 == core.-X=1).
// core connects to core on any face (1==1).
// ---------------------------------------------------------------------------
static std::vector<WFCTile> makeThreeTilePalette() {
    WFCTile air;
    air.index = 0;
    air.weight = 1.0f;
    air.sockets = {0, 0, 0, 0, 0, 0};

    WFCTile border;
    border.index = 1;
    border.weight = 1.0f;
    border.sockets = {1, 0, 1, 0, 1, 0}; // +X=1,-X=0,+Y=1,-Y=0,+Z=1,-Z=0

    WFCTile core;
    core.index = 2;
    core.weight = 1.0f;
    core.sockets = {1, 1, 1, 1, 1, 1};

    return {air, border, core};
}

// ---------------------------------------------------------------------------
// 1. 2x2x1 grid with 2 tile types solves correctly
// ---------------------------------------------------------------------------
TEST(WFCGeneratorTest, TwoByTwoSolvesCorrectly) {
    auto tiles = makeTwoTilePalette();

    WFCGrid grid;
    grid.init(2, 2, 1, tiles);

    auto result = wfcSolve(grid, tiles, 42);

    // Must terminate and all cells collapsed.
    EXPECT_TRUE(grid.isFullyCollapsed());

    // Each cell should have a valid collapsed index.
    for (int y = 0; y < 2; ++y) {
        for (int x = 0; x < 2; ++x) {
            const auto& cell = grid.cellAt(x, y, 0);
            EXPECT_TRUE(cell.isCollapsed());
            EXPECT_GE(cell.collapsedIndex, 0);
            EXPECT_LT(cell.collapsedIndex, static_cast<int>(tiles.size()));
        }
    }

    // With uniform-socket tiles (air=all-0, solid=all-1), adjacent cells
    // must have matching sockets. Since air only connects to air and solid
    // only connects to solid, the entire grid should be uniform.
    int first = grid.cellAt(0, 0, 0).collapsedIndex;
    for (int y = 0; y < 2; ++y) {
        for (int x = 0; x < 2; ++x) {
            EXPECT_EQ(grid.cellAt(x, y, 0).collapsedIndex, first) << "All cells must match due to socket constraints";
        }
    }
}

// ---------------------------------------------------------------------------
// 2. Propagation removes incompatible tiles
// ---------------------------------------------------------------------------
TEST(WFCGeneratorTest, PropagationRemovesIncompatible) {
    auto tiles = makeTwoTilePalette();
    auto adj = WFCAdjacency::build(tiles);

    WFCGrid grid;
    grid.init(2, 1, 1, tiles);

    // Manually collapse cell (0,0,0) to tile 1 (solid).
    auto& left = grid.cellAt(0, 0, 0);
    left.possible.assign(2, false);
    left.possible[1] = true;
    left.collapsedIndex = 1;
    left.entropy = 0.0f;

    // Propagate from (0,0,0).
    wfcPropagate(grid, 0, 0, 0, tiles, adj);

    // Cell (1,0,0) should now only allow tile 1 (solid), because
    // solid's +X socket (1) only matches solid's -X socket (1).
    const auto& right = grid.cellAt(1, 0, 0);
    EXPECT_FALSE(right.possible[0]) << "Air should be removed by propagation";
    EXPECT_TRUE(right.possible[1]) << "Solid should remain compatible";
}

// ---------------------------------------------------------------------------
// 3. Contradiction produces air tile (index 0)
// ---------------------------------------------------------------------------
TEST(WFCGeneratorTest, ContradictionProducesAir) {
    // Build tiles where contradiction is guaranteed:
    // Tile 0 (air): sockets all 0
    // Tile 1: +X=1, all others=2  (incompatible with everything on most faces)
    // Tile 2: -X=3, all others=4  (incompatible with everything)
    //
    // On a 2x1x1 grid, if tile 1 collapses at (0,0,0), its +X socket=1
    // requires neighbor's -X socket=1. But tile 2's -X=3, tile 0's -X=0.
    // Nothing matches -> contradiction -> air.
    WFCTile air;
    air.index = 0;
    air.weight = 1.0f;
    air.sockets = {0, 0, 0, 0, 0, 0};

    WFCTile oddA;
    oddA.index = 1;
    oddA.weight = 1.0f;
    oddA.sockets = {99, 2, 2, 2, 2, 2}; // +X=99, unique

    WFCTile oddB;
    oddB.index = 2;
    oddB.weight = 1.0f;
    oddB.sockets = {3, 88, 4, 4, 4, 4}; // -X=88, unique

    std::vector<WFCTile> tiles = {air, oddA, oddB};
    auto adj = WFCAdjacency::build(tiles);

    WFCGrid grid;
    grid.init(2, 1, 1, tiles);

    // Force collapse (0,0,0) to tile 1.
    auto& left = grid.cellAt(0, 0, 0);
    left.possible.assign(3, false);
    left.possible[1] = true;
    left.collapsedIndex = 1;
    left.entropy = 0.0f;

    bool ok = wfcPropagate(grid, 0, 0, 0, tiles, adj);

    // Propagation should detect contradiction.
    EXPECT_FALSE(ok);

    // The contradicted cell should fall back to air (index 0).
    const auto& right = grid.cellAt(1, 0, 0);
    EXPECT_TRUE(right.isCollapsed());
    EXPECT_EQ(right.collapsedIndex, 0);
}

// ---------------------------------------------------------------------------
// 4. Solver terminates for all test cases (no infinite loop)
// ---------------------------------------------------------------------------
TEST(WFCGeneratorTest, SolverTerminates) {
    auto tiles = makeTwoTilePalette();

    // Test multiple grid sizes.
    for (int size : {1, 2, 3, 4, 5}) {
        WFCGrid grid;
        grid.init(size, size, 1, tiles);

        wfcSolve(grid, tiles, 123);

        EXPECT_TRUE(grid.isFullyCollapsed()) << "Grid " << size << "x" << size << "x1 should be fully collapsed";
    }
}

// ---------------------------------------------------------------------------
// 5. Same seed = identical result (deterministic)
// ---------------------------------------------------------------------------
TEST(WFCGeneratorTest, DeterministicWithSameSeed) {
    auto tiles = makeThreeTilePalette();
    constexpr uint32_t kSeed = 7777;

    // Run 1.
    WFCGrid grid1;
    grid1.init(4, 4, 2, tiles);
    wfcSolve(grid1, tiles, kSeed);

    // Run 2.
    WFCGrid grid2;
    grid2.init(4, 4, 2, tiles);
    wfcSolve(grid2, tiles, kSeed);

    // Every cell must match.
    for (int z = 0; z < 2; ++z) {
        for (int y = 0; y < 4; ++y) {
            for (int x = 0; x < 4; ++x) {
                EXPECT_EQ(grid1.cellAt(x, y, z).collapsedIndex, grid2.cellAt(x, y, z).collapsedIndex)
                    << "Mismatch at (" << x << "," << y << "," << z << ")";
            }
        }
    }
}

// ---------------------------------------------------------------------------
// 6. Entropy decreases monotonically during solve
// ---------------------------------------------------------------------------
TEST(WFCGeneratorTest, EntropyDecreasesMonotonically) {
    auto tiles = makeTwoTilePalette();

    WFCGrid grid;
    grid.init(3, 3, 1, tiles);

    std::mt19937 rng(42);
    auto adj = WFCAdjacency::build(tiles);

    // Track the maximum entropy across all uncollapsed cells at each step.
    float prevMaxEntropy = std::numeric_limits<float>::max();

    while (!grid.isFullyCollapsed()) {
        // Compute current max entropy among uncollapsed cells.
        float maxEntropy = 0.0f;
        int uncollapsed = 0;
        for (int z = 0; z < grid.depth(); ++z) {
            for (int y = 0; y < grid.height(); ++y) {
                for (int x = 0; x < grid.width(); ++x) {
                    const auto& cell = grid.cellAt(x, y, z);
                    if (!cell.isCollapsed()) {
                        maxEntropy = std::max(maxEntropy, cell.entropy);
                        ++uncollapsed;
                    }
                }
            }
        }

        // The number of uncollapsed cells should decrease each iteration.
        // Entropy of selected cell should be <= previous max.
        if (uncollapsed == 0)
            break;

        auto [x, y, z] = grid.lowestEntropyCell(rng);
        float selectedEntropy = grid.cellAt(x, y, z).entropy;
        EXPECT_LE(selectedEntropy, prevMaxEntropy + 1e-6f)
            << "Selected cell entropy should not exceed previous maximum";

        wfcCollapse(grid.cellAt(x, y, z), tiles, rng);
        wfcPropagate(grid, x, y, z, tiles, adj);

        prevMaxEntropy = maxEntropy;
    }

    EXPECT_TRUE(grid.isFullyCollapsed());
}

// ---------------------------------------------------------------------------
// 7. lowestEntropyCell returns correct cell
// ---------------------------------------------------------------------------
TEST(WFCGeneratorTest, LowestEntropyCellCorrect) {
    auto tiles = makeTwoTilePalette();

    WFCGrid grid;
    grid.init(3, 1, 1, tiles);

    // Manually reduce cell (1,0,0) to only tile 1 (but don't mark collapsed).
    // This gives it entropy 0, while others have entropy log(2).
    auto& mid = grid.cellAt(1, 0, 0);
    mid.possible[0] = false; // remove air
    mid.updateEntropy(tiles);

    // Cell (1,0,0) now has 1 possibility -> entropy 0.
    // But possibilityCount==1 with no collapsedIndex means lowestEntropyCell
    // should skip it (count <= 1 guard). So lowest entropy among cells with
    // count > 1 should be (0,0,0) or (2,0,0).
    std::mt19937 rng(1);
    auto [x, y, z] = grid.lowestEntropyCell(rng);

    // Should be one of the two fully-open cells.
    EXPECT_TRUE((x == 0 || x == 2)) << "Expected cell (0,0,0) or (2,0,0), got (" << x << "," << y << "," << z << ")";
    EXPECT_EQ(y, 0);
    EXPECT_EQ(z, 0);
}

// ---------------------------------------------------------------------------
// 8. Adjacency build produces correct compatibility
// ---------------------------------------------------------------------------
TEST(WFCGeneratorTest, AdjacencyBuildCorrect) {
    auto tiles = makeTwoTilePalette();
    auto adj = WFCAdjacency::build(tiles);

    // On +X face: tile 0 (socket +X=0) is compatible with tiles whose -X socket=0.
    // Tile 0 has -X=0 -> compatible. Tile 1 has -X=1 -> incompatible.
    ASSERT_EQ(adj.compatible[WFCFace::PosX][0].size(), 1u);
    EXPECT_EQ(adj.compatible[WFCFace::PosX][0][0], 0);

    // Tile 1 (socket +X=1) is compatible with tiles whose -X socket=1.
    // Only tile 1 has -X=1.
    ASSERT_EQ(adj.compatible[WFCFace::PosX][1].size(), 1u);
    EXPECT_EQ(adj.compatible[WFCFace::PosX][1][0], 1);
}

// ---------------------------------------------------------------------------
// 9. Opposite face function
// ---------------------------------------------------------------------------
TEST(WFCGeneratorTest, OppositeFaceCorrect) {
    EXPECT_EQ(wfcOppositeFace(WFCFace::PosX), WFCFace::NegX);
    EXPECT_EQ(wfcOppositeFace(WFCFace::NegX), WFCFace::PosX);
    EXPECT_EQ(wfcOppositeFace(WFCFace::PosY), WFCFace::NegY);
    EXPECT_EQ(wfcOppositeFace(WFCFace::NegY), WFCFace::PosY);
    EXPECT_EQ(wfcOppositeFace(WFCFace::PosZ), WFCFace::NegZ);
    EXPECT_EQ(wfcOppositeFace(WFCFace::NegZ), WFCFace::PosZ);
}

// ---------------------------------------------------------------------------
// 10. 3D solve: 2x2x2 grid terminates and is consistent
// ---------------------------------------------------------------------------
TEST(WFCGeneratorTest, ThreeDimensionalSolve) {
    auto tiles = makeTwoTilePalette();

    WFCGrid grid;
    grid.init(2, 2, 2, tiles);

    auto result = wfcSolve(grid, tiles, 999);
    EXPECT_TRUE(grid.isFullyCollapsed());

    // Verify adjacency consistency: for every adjacent pair, sockets must match.
    auto adj = WFCAdjacency::build(tiles);
    for (int z = 0; z < 2; ++z) {
        for (int y = 0; y < 2; ++y) {
            for (int x = 0; x < 2; ++x) {
                int tileIdx = grid.cellAt(x, y, z).collapsedIndex;
                for (int face = 0; face < 6; ++face) {
                    int nx = x + kWFCNeighborOffsets[face][0];
                    int ny = y + kWFCNeighborOffsets[face][1];
                    int nz = z + kWFCNeighborOffsets[face][2];
                    if (nx < 0 || nx >= 2 || ny < 0 || ny >= 2 || nz < 0 || nz >= 2)
                        continue;

                    int neighborIdx = grid.cellAt(nx, ny, nz).collapsedIndex;
                    int opp = wfcOppositeFace(face);
                    EXPECT_EQ(tiles[tileIdx].sockets[face], tiles[neighborIdx].sockets[opp])
                        << "Socket mismatch at (" << x << "," << y << "," << z << ") face " << face << " neighbor ("
                        << nx << "," << ny << "," << nz << ")";
                }
            }
        }
    }
}

// ---------------------------------------------------------------------------
// 11. Single cell grid collapses immediately
// ---------------------------------------------------------------------------
TEST(WFCGeneratorTest, SingleCellCollapse) {
    auto tiles = makeTwoTilePalette();

    WFCGrid grid;
    grid.init(1, 1, 1, tiles);

    auto result = wfcSolve(grid, tiles, 0);

    EXPECT_TRUE(grid.isFullyCollapsed());
    EXPECT_EQ(result, WFCResult::Success);
    EXPECT_TRUE(grid.cellAt(0, 0, 0).isCollapsed());
}

// ---------------------------------------------------------------------------
// 12. Weighted tiles: heavier tile chosen more often (statistical)
// ---------------------------------------------------------------------------
TEST(WFCGeneratorTest, WeightedSelectionBias) {
    // Single cell, two tiles: tile 0 weight=1, tile 1 weight=99.
    WFCTile light;
    light.index = 0;
    light.weight = 1.0f;
    light.sockets = {0, 0, 0, 0, 0, 0};

    WFCTile heavy;
    heavy.index = 1;
    heavy.weight = 99.0f;
    heavy.sockets = {0, 0, 0, 0, 0, 0};

    std::vector<WFCTile> tiles = {light, heavy};

    int heavyCount = 0;
    constexpr int kTrials = 200;

    for (int i = 0; i < kTrials; ++i) {
        WFCGrid grid;
        grid.init(1, 1, 1, tiles);
        wfcSolve(grid, tiles, static_cast<uint32_t>(i));
        if (grid.cellAt(0, 0, 0).collapsedIndex == 1)
            ++heavyCount;
    }

    // With 99:1 weight ratio, we expect ~99% heavy. Allow generous margin.
    EXPECT_GT(heavyCount, kTrials * 80 / 100) << "Heavy tile (weight 99) should be chosen most of the time";
}

// ===========================================================================
// TileSet tests (EF-22.2)
// ===========================================================================

// ---------------------------------------------------------------------------
// 13. Dungeon tile set has >= 5 tiles, Building tile set has >= 5 tiles
// ---------------------------------------------------------------------------
TEST(WFCGeneratorTest, TileSetMinimumTileCount) {
    auto dungeon = createDungeonTileSet();
    EXPECT_GE(dungeon.tiles.size(), 5u) << "Dungeon tile set must have at least 5 tiles";

    auto building = createBuildingTileSet();
    EXPECT_GE(building.tiles.size(), 5u) << "Building tile set must have at least 5 tiles";
}

// ---------------------------------------------------------------------------
// 14. No orphan sockets (every socket value on any face has at least one
//     matching tile on the opposite face)
// ---------------------------------------------------------------------------
TEST(WFCGeneratorTest, NoOrphanSockets) {
    auto checkOrphans = [](const WFCTileSet& ts, const std::string& label) {
        for (int face = 0; face < 6; ++face) {
            int opp = wfcOppositeFace(face);
            for (size_t i = 0; i < ts.tiles.size(); ++i) {
                int socket = ts.tiles[i].sockets[face];
                bool found = false;
                for (size_t j = 0; j < ts.tiles.size(); ++j) {
                    if (ts.tiles[j].sockets[opp] == socket) {
                        found = true;
                        break;
                    }
                }
                EXPECT_TRUE(found) << label << ": tile \"" << ts.tiles[i].name << "\" has orphan socket " << socket
                                   << " on face " << face;
            }
        }
    };

    checkOrphans(createDungeonTileSet(), "dungeon");
    checkOrphans(createBuildingTileSet(), "building");
}

// ---------------------------------------------------------------------------
// 15. All tile densities are in [0, 1]
// ---------------------------------------------------------------------------
TEST(WFCGeneratorTest, TileDataDensityValid) {
    auto check = [](const WFCTileSet& ts, const std::string& label) {
        for (const auto& tile : ts.tiles) {
            for (int i = 0; i < kWFCTileVolume; ++i) {
                EXPECT_GE(tile.density[i], 0.0f)
                    << label << ": tile \"" << tile.name << "\" density[" << i << "] below 0";
                EXPECT_LE(tile.density[i], 1.0f)
                    << label << ": tile \"" << tile.name << "\" density[" << i << "] above 1";
            }
        }
    };

    check(createDungeonTileSet(), "dungeon");
    check(createBuildingTileSet(), "building");
}

// ---------------------------------------------------------------------------
// 16. Adjacency derived correctly: pairs are non-empty and consistent
// ---------------------------------------------------------------------------
TEST(WFCGeneratorTest, AdjacencyDerivedCorrectly) {
    auto dungeon = createDungeonTileSet();
    EXPECT_FALSE(dungeon.adjacencyPairs.empty()) << "Dungeon adjacency pairs should not be empty";

    // Every pair index must be a valid tile index.
    int n = static_cast<int>(dungeon.tiles.size());
    for (const auto& [a, b] : dungeon.adjacencyPairs) {
        EXPECT_GE(a, 0);
        EXPECT_LT(a, n);
        EXPECT_GE(b, 0);
        EXPECT_LT(b, n);
    }

    // Verify: the air tile (socket 0 on all faces) should be self-adjacent
    // on every face, producing at least 6 (air, air) pairs.
    int airPairCount = 0;
    for (const auto& [a, b] : dungeon.adjacencyPairs) {
        if (a == 0 && b == 0)
            ++airPairCount;
    }
    EXPECT_GE(airPairCount, 6) << "Air tile should be self-adjacent on all 6 faces";

    // Also check building tile set.
    auto building = createBuildingTileSet();
    EXPECT_FALSE(building.adjacencyPairs.empty()) << "Building adjacency pairs should not be empty";
}

// ---------------------------------------------------------------------------
// 17. kWFCTileSize constant is correct
// ---------------------------------------------------------------------------
TEST(WFCGeneratorTest, TileSizeConstant) {
    EXPECT_EQ(kWFCTileSize, 4);
    EXPECT_EQ(kWFCTileVolume, 64);
}

// ===========================================================================
// WFCTerrainGenerator tests (EF-22.3)
// ===========================================================================

// ---------------------------------------------------------------------------
// Helper: create a default WFCTerrainConfig with dungeon tileset.
// ---------------------------------------------------------------------------
static WFCTerrainConfig makeDefaultTerrainConfig() {
    WFCTerrainConfig cfg;
    cfg.seed = 42;
    cfg.tilesX = 4;
    cfg.tilesY = 4;
    cfg.tilesZ = 4;
    cfg.tileset = createDungeonTileSet();
    return cfg;
}

// ---------------------------------------------------------------------------
// Helper: create an AABB that covers the full tile grid (tilesN * kWFCTileSize).
// ---------------------------------------------------------------------------
static AABB makeFullRegion(const WFCTerrainConfig& cfg) {
    Vec3f minPt(0.0f, 0.0f, 0.0f);
    Vec3f maxPt(static_cast<float>(cfg.tilesX * kWFCTileSize), static_cast<float>(cfg.tilesY * kWFCTileSize),
                static_cast<float>(cfg.tilesZ * kWFCTileSize));
    return AABB(minPt, maxPt);
}

// ---------------------------------------------------------------------------
// 18. WFCTerrainGenerator produces non-zero density in output fields
// ---------------------------------------------------------------------------
TEST(WFCGeneratorTest, TerrainGeneratorProducesNonZeroDensity) {
    auto cfg = makeDefaultTerrainConfig();
    WFCTerrainGenerator gen(cfg);

    DensityField density;
    EssenceField essence;
    AABB region = makeFullRegion(cfg);

    gen.generate(density, essence, region);

    // At least some voxels should have non-zero density (dungeon tileset
    // has wall, corridor, room tiles with density > 0).
    int nonZeroCount = 0;
    int maxX = cfg.tilesX * kWFCTileSize;
    int maxY = cfg.tilesY * kWFCTileSize;
    int maxZ = cfg.tilesZ * kWFCTileSize;
    for (int z = 0; z < maxZ; ++z) {
        for (int y = 0; y < maxY; ++y) {
            for (int x = 0; x < maxX; ++x) {
                if (density.read(x, y, z) > 0.0f)
                    ++nonZeroCount;
            }
        }
    }

    EXPECT_GT(nonZeroCount, 0) << "WFCTerrainGenerator should produce at least some non-zero density voxels";
}

// ---------------------------------------------------------------------------
// 19. Output is within AABB bounds (no writes outside region)
// ---------------------------------------------------------------------------
TEST(WFCGeneratorTest, TerrainOutputWithinBounds) {
    auto cfg = makeDefaultTerrainConfig();
    cfg.tilesX = 2;
    cfg.tilesY = 2;
    cfg.tilesZ = 2;

    WFCTerrainGenerator gen(cfg);

    DensityField density;
    EssenceField essence;

    // Use a region that exactly fits the tile grid.
    AABB region = makeFullRegion(cfg);
    gen.generate(density, essence, region);

    int maxX = cfg.tilesX * kWFCTileSize;
    int maxY = cfg.tilesY * kWFCTileSize;
    int maxZ = cfg.tilesZ * kWFCTileSize;

    // Check voxels just outside the region in each direction.
    // They should remain at default (0).
    for (int z = 0; z < maxZ; ++z) {
        for (int y = 0; y < maxY; ++y) {
            EXPECT_EQ(density.read(-1, y, z), 0.0f) << "No writes expected at x=-1";
            EXPECT_EQ(density.read(maxX, y, z), 0.0f) << "No writes expected at x=" << maxX;
        }
    }
    for (int z = 0; z < maxZ; ++z) {
        for (int x = 0; x < maxX; ++x) {
            EXPECT_EQ(density.read(x, -1, z), 0.0f) << "No writes expected at y=-1";
            EXPECT_EQ(density.read(x, maxY, z), 0.0f) << "No writes expected at y=" << maxY;
        }
    }
    for (int y = 0; y < maxY; ++y) {
        for (int x = 0; x < maxX; ++x) {
            EXPECT_EQ(density.read(x, y, -1), 0.0f) << "No writes expected at z=-1";
            EXPECT_EQ(density.read(x, y, maxZ), 0.0f) << "No writes expected at z=" << maxZ;
        }
    }
}

// ---------------------------------------------------------------------------
// 20. Deterministic: same config = same output
// ---------------------------------------------------------------------------
TEST(WFCGeneratorTest, TerrainDeterministic) {
    auto cfg = makeDefaultTerrainConfig();
    cfg.seed = 1234;

    AABB region = makeFullRegion(cfg);

    // Run 1.
    DensityField density1;
    EssenceField essence1;
    WFCTerrainGenerator gen1(cfg);
    gen1.generate(density1, essence1, region);

    // Run 2.
    DensityField density2;
    EssenceField essence2;
    WFCTerrainGenerator gen2(cfg);
    gen2.generate(density2, essence2, region);

    // Every voxel must match.
    int maxX = cfg.tilesX * kWFCTileSize;
    int maxY = cfg.tilesY * kWFCTileSize;
    int maxZ = cfg.tilesZ * kWFCTileSize;
    for (int z = 0; z < maxZ; ++z) {
        for (int y = 0; y < maxY; ++y) {
            for (int x = 0; x < maxX; ++x) {
                EXPECT_EQ(density1.read(x, y, z), density2.read(x, y, z))
                    << "Density mismatch at (" << x << "," << y << "," << z << ")";
                auto e1 = essence1.read(x, y, z);
                auto e2 = essence2.read(x, y, z);
                EXPECT_EQ(e1.x, e2.x) << "Essence.x mismatch at (" << x << "," << y << "," << z << ")";
                EXPECT_EQ(e1.y, e2.y) << "Essence.y mismatch at (" << x << "," << y << "," << z << ")";
                EXPECT_EQ(e1.z, e2.z) << "Essence.z mismatch at (" << x << "," << y << "," << z << ")";
                EXPECT_EQ(e1.w, e2.w) << "Essence.w mismatch at (" << x << "," << y << "," << z << ")";
            }
        }
    }
}

// ---------------------------------------------------------------------------
// 21. Blending preserves existing terrain (pre-fill density, verify max behavior)
// ---------------------------------------------------------------------------
TEST(WFCGeneratorTest, TerrainBlendingPreservesExisting) {
    auto cfg = makeDefaultTerrainConfig();
    cfg.tilesX = 2;
    cfg.tilesY = 2;
    cfg.tilesZ = 2;

    WFCTerrainGenerator gen(cfg);

    DensityField density;
    EssenceField essence;
    AABB region = makeFullRegion(cfg);

    int maxX = cfg.tilesX * kWFCTileSize;
    int maxY = cfg.tilesY * kWFCTileSize;
    int maxZ = cfg.tilesZ * kWFCTileSize;

    // Pre-fill density with a high value (0.95).
    density.fill(0, 0, 0, maxX - 1, maxY - 1, maxZ - 1, 0.95f);

    gen.generate(density, essence, region);

    // Every voxel should be >= 0.95 because blending uses max(existing, tile).
    // Tile densities are in [0, 1], so max(0.95, tile) >= 0.95 always.
    for (int z = 0; z < maxZ; ++z) {
        for (int y = 0; y < maxY; ++y) {
            for (int x = 0; x < maxX; ++x) {
                EXPECT_GE(density.read(x, y, z), 0.95f)
                    << "Blending should preserve existing density at (" << x << "," << y << "," << z << ")";
            }
        }
    }
}

// ---------------------------------------------------------------------------
// 22. Essence written only where tile has non-zero density
// ---------------------------------------------------------------------------
TEST(WFCGeneratorTest, TerrainEssenceOnlyWhereNonZeroDensity) {
    auto cfg = makeDefaultTerrainConfig();
    cfg.tilesX = 2;
    cfg.tilesY = 2;
    cfg.tilesZ = 2;

    WFCTerrainGenerator gen(cfg);

    DensityField density;
    EssenceField essence;
    AABB region = makeFullRegion(cfg);

    gen.generate(density, essence, region);

    int maxX = cfg.tilesX * kWFCTileSize;
    int maxY = cfg.tilesY * kWFCTileSize;
    int maxZ = cfg.tilesZ * kWFCTileSize;

    // Run a second pass to verify: generate with a fresh field, then check
    // that wherever density is 0 (from a tile with zero density), essence
    // is also the default (0,0,0,0).
    // We need to check carefully: density uses max blending, so we need
    // to start with zero density to see which tiles wrote zero.
    DensityField densityCheck;
    EssenceField essenceCheck;
    WFCTerrainGenerator gen2(cfg);
    gen2.generate(densityCheck, essenceCheck, region);

    for (int z = 0; z < maxZ; ++z) {
        for (int y = 0; y < maxY; ++y) {
            for (int x = 0; x < maxX; ++x) {
                if (densityCheck.read(x, y, z) == 0.0f) {
                    // If density is zero, essence should be default (0,0,0,0).
                    auto e = essenceCheck.read(x, y, z);
                    EXPECT_EQ(e.x, 0.0f) << "Essence.x should be 0 at zero-density voxel (" << x << "," << y << "," << z
                                         << ")";
                    EXPECT_EQ(e.y, 0.0f) << "Essence.y should be 0 at zero-density voxel";
                    EXPECT_EQ(e.z, 0.0f) << "Essence.z should be 0 at zero-density voxel";
                    EXPECT_EQ(e.w, 0.0f) << "Essence.w should be 0 at zero-density voxel";
                }
            }
        }
    }
}

// ---------------------------------------------------------------------------
// 23. Default config (dungeon tileset) works end-to-end
// ---------------------------------------------------------------------------
TEST(WFCGeneratorTest, TerrainDefaultConfigEndToEnd) {
    WFCTerrainConfig cfg;
    cfg.tileset = createDungeonTileSet();

    WFCTerrainGenerator gen(cfg);

    DensityField density;
    EssenceField essence;
    AABB region = makeFullRegion(cfg);

    // Should not crash or infinite loop.
    gen.generate(density, essence, region);

    // Verify config accessors.
    EXPECT_EQ(gen.config().seed, 42u);
    EXPECT_EQ(gen.config().tilesX, 4);
    EXPECT_EQ(gen.config().tilesY, 4);
    EXPECT_EQ(gen.config().tilesZ, 4);

    // setConfig should work.
    WFCTerrainConfig cfg2 = cfg;
    cfg2.seed = 9999;
    gen.setConfig(cfg2);
    EXPECT_EQ(gen.config().seed, 9999u);
}

// ---------------------------------------------------------------------------
// 24. Region clipping: tiles partially outside AABB are clipped correctly
// ---------------------------------------------------------------------------
TEST(WFCGeneratorTest, TerrainRegionClipping) {
    auto cfg = makeDefaultTerrainConfig();
    cfg.tilesX = 2;
    cfg.tilesY = 2;
    cfg.tilesZ = 2;

    WFCTerrainGenerator gen(cfg);

    DensityField density;
    EssenceField essence;

    // Create a region smaller than the full tile grid.
    // Full grid would be 8x8x8, but we clip to 0..5 on each axis.
    int clipMax = 5;
    AABB region(Vec3f(0.0f, 0.0f, 0.0f),
                Vec3f(static_cast<float>(clipMax), static_cast<float>(clipMax), static_cast<float>(clipMax)));

    gen.generate(density, essence, region);

    // Voxels at x/y/z >= clipMax should not have been written.
    int fullMax = cfg.tilesX * kWFCTileSize; // 8
    for (int z = clipMax; z < fullMax; ++z) {
        for (int y = clipMax; y < fullMax; ++y) {
            for (int x = clipMax; x < fullMax; ++x) {
                EXPECT_EQ(density.read(x, y, z), 0.0f)
                    << "Voxel at (" << x << "," << y << "," << z << ") should be clipped (outside region)";
            }
        }
    }

    // But voxels within the region may have non-zero density.
    int nonZero = 0;
    for (int z = 0; z < clipMax; ++z) {
        for (int y = 0; y < clipMax; ++y) {
            for (int x = 0; x < clipMax; ++x) {
                if (density.read(x, y, z) > 0.0f)
                    ++nonZero;
            }
        }
    }
    EXPECT_GT(nonZero, 0) << "Some voxels within the clipped region should have non-zero density";
}
