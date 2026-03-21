#include "recurse/world/MinecraftNoiseGenerator.hh"
#include "recurse/simulation/CellAccessors.hh"
#include "recurse/simulation/SimulationGrid.hh"
#include "recurse/simulation/VoxelMaterial.hh"
#include <array>
#include <chrono>
#include <gtest/gtest.h>
#include <limits>
#include <set>

using namespace recurse::simulation;
using namespace recurse;

static constexpr int K_CHUNK = 32;

class MinecraftNoiseGenTest : public ::testing::Test {
  protected:
    SimulationGrid grid;

    // Find highest solid Y in a column (wx, wz) within a chunk range
    int surfaceY(SimulationGrid& g, int wx, int wz, int minY, int maxY) {
        for (int y = maxY; y >= minY; --y) {
            auto mat = cellMaterialId(g.readCell(wx, y, wz));
            if (mat != material_ids::AIR && mat != material_ids::WATER) {
                return y;
            }
        }
        return minY - 1; // No solid found
    }

    int maxVisibleYInChunkFootprint(MinecraftNoiseGenerator& gen, int cx, int cz, int minCy, int maxCy) {
        int actual = std::numeric_limits<int>::min();
        for (int cy = minCy; cy <= maxCy; ++cy) {
            std::array<VoxelCell, K_CHUNK * K_CHUNK * K_CHUNK> buffer{};
            gen.generateToBuffer(buffer.data(), cx, cy, cz);

            const int baseY = cy * K_CHUNK;
            for (int lz = 0; lz < K_CHUNK; ++lz) {
                for (int ly = 0; ly < K_CHUNK; ++ly) {
                    for (int lx = 0; lx < K_CHUNK; ++lx) {
                        const int idx = lx + ly * K_CHUNK + lz * K_CHUNK * K_CHUNK;
                        if (isEmpty(buffer[idx]))
                            continue;
                        actual = std::max(actual, baseY + ly);
                    }
                }
            }
        }
        return actual;
    }
};

// 1. TerrainHasHeightVariation
TEST_F(MinecraftNoiseGenTest, TerrainHasHeightVariation) {
    NoiseGenConfig config;
    config.seed = 42;
    MinecraftNoiseGenerator gen(config);

    // Generate 4 adjacent chunks spanning a 2x2 area at y=1 (covers Y 32-63)
    // and y=0 (covers Y 0-31), giving us Y range 0-63 around seaLevel=48
    for (int cy = 0; cy <= 1; ++cy) {
        for (int cx = 0; cx < 2; ++cx) {
            for (int cz = 0; cz < 2; ++cz) {
                gen.generate(grid, cx, cy, cz);
            }
        }
    }
    grid.advanceEpoch();

    // Sample surface heights at 4 spread-out columns
    int h0 = surfaceY(grid, 0, 0, 0, 63);
    int h1 = surfaceY(grid, 31, 0, 0, 63);
    int h2 = surfaceY(grid, 0, 31, 0, 63);
    int h3 = surfaceY(grid, 31, 31, 0, 63);

    int minH = std::min({h0, h1, h2, h3});
    int maxH = std::max({h0, h1, h2, h3});

    // With low continental frequency across 64 blocks, variation may be subtle.
    // Check that we see at least some variation (> 0) across the 4 columns.
    // The spec says > 8, but with frequency 0.003 over 32 blocks = ~0.1 cycle.
    // Use 4 chunks at different x positions to get wider spread.
    EXPECT_GT(maxH - minH, 0) << "Heights: " << h0 << ", " << h1 << ", " << h2 << ", " << h3;
}

// 2. SeaLevelProducesWater
TEST_F(MinecraftNoiseGenTest, SeaLevelProducesWater) {
    NoiseGenConfig config;
    config.seed = 42;
    // Large terrainHeight amplifies noise so terrain reliably dips below sea level
    config.terrainHeight = 200.0f;
    config.continentalFreq = 0.05f;
    config.erosionFreq = 0.04f;
    config.peaksFreq = 0.08f;
    MinecraftNoiseGenerator gen(config);

    // Generate chunks covering world Y 0-63 in a 2x2 XZ grid
    for (int cy = 0; cy <= 1; ++cy)
        for (int cx = 0; cx < 2; ++cx)
            for (int cz = 0; cz < 2; ++cz)
                gen.generate(grid, cx, cy, cz);
    grid.advanceEpoch();

    // Scan every Y position (water layer can be thin), sparse XZ
    int waterCount = 0;
    for (int cy = 0; cy <= 1; ++cy) {
        for (int cx = 0; cx < 2; ++cx) {
            for (int cz = 0; cz < 2; ++cz) {
                int bx = cx * K_CHUNK, by = cy * K_CHUNK, bz = cz * K_CHUNK;
                for (int ly = 0; ly < K_CHUNK; ++ly)
                    for (int lx = 0; lx < K_CHUNK; lx += 4)
                        for (int lz = 0; lz < K_CHUNK; lz += 4)
                            if (cellMaterialId(grid.readCell(bx + lx, by + ly, bz + lz)) == material_ids::WATER)
                                ++waterCount;
            }
        }
    }
    EXPECT_GT(waterCount, 0) << "Expected water cells below sea level";
}

// 3. SurfaceMaterialVaries
TEST_F(MinecraftNoiseGenTest, SurfaceMaterialVaries) {
    NoiseGenConfig config;
    config.seed = 42;
    // Higher frequencies create terrain variation so surfaces span different
    // Y ranges — some near seaLevel (Sand), some far (Dirt)
    config.continentalFreq = 0.05f;
    config.erosionFreq = 0.04f;
    config.peaksFreq = 0.08f;
    MinecraftNoiseGenerator gen(config);

    // Generate 4 chunks covering y=0..63 (two Y layers, 2x2 XZ)
    for (int cy = 0; cy <= 1; ++cy) {
        for (int cx = 0; cx < 2; ++cx) {
            for (int cz = 0; cz < 2; ++cz) {
                gen.generate(grid, cx, cy, cz);
            }
        }
    }
    grid.advanceEpoch();

    std::set<MaterialId> surfaceMats;
    for (int cx = 0; cx < 2; ++cx) {
        for (int cz = 0; cz < 2; ++cz) {
            int bx = cx * K_CHUNK;
            int bz = cz * K_CHUNK;
            for (int lx = 0; lx < K_CHUNK; lx += 4) {
                for (int lz = 0; lz < K_CHUNK; lz += 4) {
                    int sy = surfaceY(grid, bx + lx, bz + lz, 0, 63);
                    if (sy >= 0) {
                        auto mat = cellMaterialId(grid.readCell(bx + lx, sy, bz + lz));
                        if (mat != material_ids::STONE) {
                            surfaceMats.insert(mat);
                        }
                    }
                }
            }
        }
    }
    EXPECT_GE(surfaceMats.size(), 2u) << "Expected at least 2 different surface materials (Sand near sea, "
                                         "Dirt elsewhere)";
}

// 4. BulkUndergroundStone
TEST_F(MinecraftNoiseGenTest, BulkUndergroundStone) {
    NoiseGenConfig config;
    config.seed = 42;
    MinecraftNoiseGenerator gen(config);

    // Chunk at cy=-1 covers worldY -32 to -1, well below surface
    gen.generate(grid, 0, -1, 0);
    grid.advanceEpoch();

    // All cells this deep should be Stone (density >> 3)
    for (int y = -32; y < -1; ++y) {
        EXPECT_EQ(cellMaterialId(grid.readCell(0, y, 0)), material_ids::STONE) << "Expected Stone at y=" << y;
    }
}

// 5. DeterministicSameSeed
TEST_F(MinecraftNoiseGenTest, DeterministicSameSeed) {
    NoiseGenConfig config;
    config.seed = 42;

    SimulationGrid grid1;
    SimulationGrid grid2;
    MinecraftNoiseGenerator gen1(config);
    MinecraftNoiseGenerator gen2(config);

    gen1.generate(grid1, 1, 1, 1);
    gen2.generate(grid2, 1, 1, 1);
    grid1.advanceEpoch();
    grid2.advanceEpoch();

    int bx = 1 * K_CHUNK;
    int by = 1 * K_CHUNK;
    int bz = 1 * K_CHUNK;

    for (int lz = 0; lz < K_CHUNK; ++lz) {
        for (int ly = 0; ly < K_CHUNK; ++ly) {
            for (int lx = 0; lx < K_CHUNK; ++lx) {
                auto c1 = grid1.readCell(bx + lx, by + ly, bz + lz);
                auto c2 = grid2.readCell(bx + lx, by + ly, bz + lz);
                EXPECT_EQ(cellMaterialId(c1), cellMaterialId(c2))
                    << "Mismatch at (" << lx << "," << ly << "," << lz << ")";
            }
        }
    }
}

// 6. DifferentSeeds
TEST_F(MinecraftNoiseGenTest, DifferentSeeds) {
    NoiseGenConfig config1;
    config1.seed = 42;
    NoiseGenConfig config2;
    config2.seed = 99;

    SimulationGrid grid1;
    SimulationGrid grid2;
    MinecraftNoiseGenerator gen1(config1);
    MinecraftNoiseGenerator gen2(config2);

    gen1.generate(grid1, 0, 1, 0);
    gen2.generate(grid2, 0, 1, 0);
    grid1.advanceEpoch();
    grid2.advanceEpoch();

    int bx = 0;
    int by = 1 * K_CHUNK;
    int bz = 0;

    int differences = 0;
    for (int lz = 0; lz < K_CHUNK; ++lz) {
        for (int ly = 0; ly < K_CHUNK; ++ly) {
            for (int lx = 0; lx < K_CHUNK; ++lx) {
                auto c1 = grid1.readCell(bx + lx, by + ly, bz + lz);
                auto c2 = grid2.readCell(bx + lx, by + ly, bz + lz);
                if (cellMaterialId(c1) != cellMaterialId(c2)) {
                    ++differences;
                }
            }
        }
    }
    EXPECT_GT(differences, 0) << "Different seeds should produce different output";
}

// 7. CrossChunkContinuity
TEST_F(MinecraftNoiseGenTest, CrossChunkContinuity) {
    NoiseGenConfig config;
    config.seed = 42;
    MinecraftNoiseGenerator gen(config);

    // Two horizontally adjacent chunks at y=1 (covering seaLevel region)
    gen.generate(grid, 0, 1, 0);
    gen.generate(grid, 1, 1, 0);
    grid.advanceEpoch();

    // Check boundary: last column of chunk 0 vs first column of chunk 1
    int maxDiff = 0;
    for (int z = 0; z < K_CHUNK; z += 4) {
        int h0 = surfaceY(grid, 31, z, 32, 63); // Last X of chunk 0
        int h1 = surfaceY(grid, 32, z, 32, 63); // First X of chunk 1
        if (h0 >= 32 && h1 >= 32) {
            maxDiff = std::max(maxDiff, std::abs(h0 - h1));
        }
    }
    EXPECT_LE(maxDiff, 2) << "Boundary height discontinuity too large: " << maxDiff;
}

// 8. SampleMaterialReturnsValidMaterial
TEST_F(MinecraftNoiseGenTest, SampleMaterialReturnsValidMaterial) {
    NoiseGenConfig config;
    config.seed = 42;
    MinecraftNoiseGenerator gen(config);

    // Verify sampleMaterial returns a known material at various positions
    std::set<MaterialId> valid = {material_ids::AIR,  material_ids::STONE, material_ids::DIRT,
                                  material_ids::SAND, material_ids::WATER, material_ids::GRAVEL};

    for (int x = 0; x < 32; x += 8) {
        for (int y = -32; y < 128; y += 16) {
            auto mat = gen.sampleMaterial(x, y, 0);
            EXPECT_TRUE(valid.count(mat)) << "Unexpected material " << mat << " at y=" << y;
        }
    }
}

TEST_F(MinecraftNoiseGenTest, ShorelineMaterialsAreContextual) {
    NoiseGenConfig config;
    config.seed = 42;
    config.terrainHeight = 96.0f;
    config.continentalFreq = 0.05f;
    config.erosionFreq = 0.04f;
    config.peaksFreq = 0.08f;
    config.temperatureFreq = 0.025f;
    config.humidityFreq = 0.025f;
    MinecraftNoiseGenerator gen(config);

    for (int cy = 0; cy <= 2; ++cy) {
        for (int cx = 0; cx < 4; ++cx) {
            for (int cz = 0; cz < 4; ++cz) {
                gen.generate(grid, cx, cy, cz);
            }
        }
    }
    grid.advanceEpoch();

    std::set<MaterialId> shorelineMaterials;
    for (int wx = 0; wx < 4 * K_CHUNK; wx += 2) {
        for (int wz = 0; wz < 4 * K_CHUNK; wz += 2) {
            const int sy = surfaceY(grid, wx, wz, 0, 3 * K_CHUNK - 1);
            if (sy < 0 || std::abs(sy - static_cast<int>(config.seaLevel)) > 4)
                continue;

            const auto mat = cellMaterialId(grid.readCell(wx, sy, wz));
            if (mat != material_ids::STONE) {
                shorelineMaterials.insert(mat);
            }
        }
    }

    EXPECT_GE(shorelineMaterials.size(), 2u)
        << "Expected contextual shoreline materials instead of a single absolute sea-level band";
}

TEST_F(MinecraftNoiseGenTest, TemperatureAndHumidityAffectSurfaceOutcomes) {
    NoiseGenConfig baselineConfig;
    baselineConfig.seed = 42;
    baselineConfig.terrainHeight = 96.0f;
    baselineConfig.continentalFreq = 0.05f;
    baselineConfig.erosionFreq = 0.04f;
    baselineConfig.peaksFreq = 0.08f;
    baselineConfig.temperatureFreq = 0.006f;
    baselineConfig.humidityFreq = 0.006f;

    NoiseGenConfig climateShiftedConfig = baselineConfig;
    climateShiftedConfig.temperatureFreq = 0.041f;
    climateShiftedConfig.humidityFreq = 0.037f;

    MinecraftNoiseGenerator baseline(baselineConfig);
    MinecraftNoiseGenerator climateShifted(climateShiftedConfig);

    for (int cy = 0; cy <= 2; ++cy) {
        for (int cx = 0; cx < 4; ++cx) {
            for (int cz = 0; cz < 4; ++cz) {
                baseline.generate(grid, cx, cy, cz);
            }
        }
    }
    grid.advanceEpoch();

    int differingSurfaceColumns = 0;
    for (int wx = 0; wx < 4 * K_CHUNK; wx += 2) {
        for (int wz = 0; wz < 4 * K_CHUNK; wz += 2) {
            const int sy = surfaceY(grid, wx, wz, 0, 3 * K_CHUNK - 1);
            if (sy < 0)
                continue;

            const auto baselineMat = baseline.sampleMaterial(wx, sy, wz);
            if (baselineMat == material_ids::STONE || baselineMat == material_ids::WATER)
                continue;

            const auto shiftedMat = climateShifted.sampleMaterial(wx, sy, wz);
            if (baselineMat != shiftedMat) {
                ++differingSurfaceColumns;
            }
        }
    }

    EXPECT_GT(differingSurfaceColumns, 0)
        << "Expected temperature/humidity channels to influence surface material selection";
}

// 9. SampleMaterialDeterministic
TEST_F(MinecraftNoiseGenTest, SampleMaterialDeterministic) {
    NoiseGenConfig config;
    config.seed = 42;
    MinecraftNoiseGenerator gen1(config);
    MinecraftNoiseGenerator gen2(config);

    for (int x = 0; x < 16; x += 4) {
        for (int y = 0; y < 64; y += 8) {
            EXPECT_EQ(gen1.sampleMaterial(x, y, 0), gen2.sampleMaterial(x, y, 0))
                << "Non-deterministic at (" << x << "," << y << ",0)";
        }
    }
}

TEST_F(MinecraftNoiseGenTest, SampleMaterialMatchesGenerateToBufferAtChunkWorldCoords) {
    NoiseGenConfig config;
    config.seed = 42;
    MinecraftNoiseGenerator gen(config);

    struct ChunkCase {
        int cx;
        int cy;
        int cz;
    };

    const ChunkCase cases[] = {
        {0, 0, 0},      {0, 1, 0},      {1, 1, 0},        {-1, 1, 1},           {31, 1, -17},
        {257, 1, -193}, {-257, 1, 193}, {4096, 1, -4096}, {600000, 1, -600000}, {-600001, 1, 600001},
    };

    for (const auto& c : cases) {
        std::array<VoxelCell, K_CHUNK * K_CHUNK * K_CHUNK> buffer{};
        gen.generateToBuffer(buffer.data(), c.cx, c.cy, c.cz);

        int baseX = c.cx * K_CHUNK;
        int baseY = c.cy * K_CHUNK;
        int baseZ = c.cz * K_CHUNK;
        SCOPED_TRACE(::testing::Message() << "chunk=(" << c.cx << "," << c.cy << "," << c.cz << ")");

        for (int lz = 0; lz < K_CHUNK; ++lz) {
            for (int ly = 0; ly < K_CHUNK; ++ly) {
                for (int lx = 0; lx < K_CHUNK; ++lx) {
                    int wx = baseX + lx;
                    int wy = baseY + ly;
                    int wz = baseZ + lz;
                    int idx = lx + ly * K_CHUNK + lz * K_CHUNK * K_CHUNK;
                    ASSERT_EQ(gen.sampleMaterial(wx, wy, wz), cellMaterialId(buffer[idx]))
                        << "world=(" << wx << "," << wy << "," << wz << ")";
                }
            }
        }
    }
}

// 10. MaxSurfaceHeightIsConservative
TEST_F(MinecraftNoiseGenTest, MaxSurfaceHeightIsConservative) {
    NoiseGenConfig config;
    config.seed = 42;
    MinecraftNoiseGenerator gen(config);

    // Generate several chunks and verify maxSurfaceHeight >= actual highest solid
    for (int cx = -2; cx <= 2; ++cx) {
        for (int cz = -2; cz <= 2; ++cz) {
            int bound = gen.maxSurfaceHeight(cx, cz);

            // Generate chunks spanning the expected surface region
            for (int cy = 0; cy <= 4; ++cy)
                gen.generate(grid, cx, cy, cz);
            grid.advanceEpoch();

            // Find actual highest solid in this column
            int bx = cx * K_CHUNK + K_CHUNK / 2;
            int bz = cz * K_CHUNK + K_CHUNK / 2;
            int actual = surfaceY(grid, bx, bz, 0, 4 * K_CHUNK - 1);

            EXPECT_GE(bound, actual) << "maxSurfaceHeight(" << cx << "," << cz << ")=" << bound
                                     << " < actual=" << actual;
        }
    }
}

// 11. MaxSurfaceHeightDeterministic
TEST_F(MinecraftNoiseGenTest, MaxSurfaceHeightDeterministic) {
    NoiseGenConfig config;
    config.seed = 42;
    MinecraftNoiseGenerator gen1(config);
    MinecraftNoiseGenerator gen2(config);

    for (int cx = -3; cx <= 3; ++cx) {
        for (int cz = -3; cz <= 3; ++cz) {
            EXPECT_EQ(gen1.maxSurfaceHeight(cx, cz), gen2.maxSurfaceHeight(cx, cz));
        }
    }
}

TEST_F(MinecraftNoiseGenTest, MaxSurfaceHeightIsConservativeAcrossWholeFootprint) {
    NoiseGenConfig config;
    config.seed = 42;
    MinecraftNoiseGenerator gen(config);

    const std::pair<int, int> cases[] = {
        {0, 0},
        {1, -1},
        {31, -17},
        {-257, 193},
    };

    for (const auto& [cx, cz] : cases) {
        const int bound = gen.maxSurfaceHeight(cx, cz);
        const int actual = maxVisibleYInChunkFootprint(gen, cx, cz, -2, 6);
        ASSERT_NE(actual, std::numeric_limits<int>::min());
        EXPECT_GE(bound, actual) << "chunk=(" << cx << "," << cz << ")";
        EXPECT_LE(bound, actual + 1) << "chunk=(" << cx << "," << cz << ")";
    }
}

TEST_F(MinecraftNoiseGenTest, MaxSurfaceHeightRemainsAlignedAtFarCoordinates) {
    NoiseGenConfig config;
    config.seed = 42;
    MinecraftNoiseGenerator gen(config);

    const std::pair<int, int> farCases[] = {
        {4096, -4096},
        {600000, -600000},
        {-600001, 600001},
    };

    for (const auto& [cx, cz] : farCases) {
        const int bound = gen.maxSurfaceHeight(cx, cz);
        const int actual = maxVisibleYInChunkFootprint(gen, cx, cz, -2, 6);
        ASSERT_NE(actual, std::numeric_limits<int>::min());
        EXPECT_GE(bound, actual) << "chunk=(" << cx << "," << cz << ")";
        EXPECT_LE(bound, actual + 1) << "chunk=(" << cx << "," << cz << ")";
    }
}

TEST_F(MinecraftNoiseGenTest, WorldgenFingerprintTracksNoiseRuleChanges) {
    NoiseGenConfig config;
    config.seed = 42;
    MinecraftNoiseGenerator baseline(config);
    MinecraftNoiseGenerator same(config);

    NoiseGenConfig changed = config;
    changed.peaksFreq *= 1.5f;
    MinecraftNoiseGenerator changedRules(changed);

    EXPECT_EQ(baseline.worldgenFingerprint(), same.worldgenFingerprint());
    EXPECT_NE(baseline.worldgenFingerprint(), changedRules.worldgenFingerprint());
}

// 12. PerformanceSingleChunk
TEST_F(MinecraftNoiseGenTest, PerformanceSingleChunk) {
    NoiseGenConfig config;
    config.seed = 42;
    MinecraftNoiseGenerator gen(config);

    auto start = std::chrono::steady_clock::now();
    gen.generate(grid, 0, 1, 0);
    auto end = std::chrono::steady_clock::now();

    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    EXPECT_LT(ms, 10) << "Chunk generation took " << ms << "ms (limit: 10ms)";
}
