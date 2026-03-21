#include "recurse/world/ChunkDensityCache.hh"
#include "recurse/simulation/VoxelConstants.hh"
#include <gtest/gtest.h>

using namespace recurse;
using recurse::simulation::K_CHUNK_SIZE;

class ChunkDensityCacheTest : public ::testing::Test {
  protected:
    ChunkDensityCache cache;
    ChunkedGrid<float, 32> density;
};

TEST_F(ChunkDensityCacheTest, InteriorMatchesGrid) {
    // Fill chunk (0,0,0) with known values
    for (int lz = 0; lz < K_CHUNK_SIZE; ++lz)
        for (int ly = 0; ly < K_CHUNK_SIZE; ++ly)
            for (int lx = 0; lx < K_CHUNK_SIZE; ++lx)
                density.set(lx, ly, lz, static_cast<float>(lx + ly * 100 + lz * 10000));

    cache.build(0, 0, 0, density);

    // Cache offset +1 maps to chunk local [0..31]
    for (int lz = 0; lz < K_CHUNK_SIZE; ++lz)
        for (int ly = 0; ly < K_CHUNK_SIZE; ++ly)
            for (int lx = 0; lx < K_CHUNK_SIZE; ++lx)
                EXPECT_FLOAT_EQ(cache.at(lx + 1, ly + 1, lz + 1), density.get(lx, ly, lz));
}

TEST_F(ChunkDensityCacheTest, BorderMatchesNeighbor) {
    // Set density at world (-1, 0, 0) which lives in chunk(-1, 0, 0)
    density.set(-1, 0, 0, 0.7f);
    cache.build(0, 0, 0, density);

    // Cache local (0, 1, 1) maps to world (-1, 0, 0)
    EXPECT_FLOAT_EQ(cache.at(0, 1, 1), 0.7f);
}

TEST_F(ChunkDensityCacheTest, MissingNeighborDefaultsToZero) {
    // Build cache for chunk(0,0,0) without loading any neighbors
    cache.build(0, 0, 0, density);

    // Border cells should be 0.0f (ChunkedGrid returns T{} for missing chunks)
    EXPECT_FLOAT_EQ(cache.at(0, 0, 0), 0.0f);
    EXPECT_FLOAT_EQ(cache.at(0, 1, 1), 0.0f);
    EXPECT_FLOAT_EQ(cache.at(1, 0, 1), 0.0f);
    EXPECT_FLOAT_EQ(cache.at(1, 1, 0), 0.0f);
}

TEST_F(ChunkDensityCacheTest, TrilinearAtIntegerMatchesDirect) {
    for (int lz = 0; lz < K_CHUNK_SIZE; ++lz)
        for (int ly = 0; ly < K_CHUNK_SIZE; ++ly)
            for (int lx = 0; lx < K_CHUNK_SIZE; ++lx)
                density.set(lx, ly, lz, static_cast<float>(lx * ly + lz));

    cache.build(0, 0, 0, density);

    EXPECT_FLOAT_EQ(cache.sample(5.0f, 5.0f, 5.0f), cache.at(5, 5, 5));
    EXPECT_FLOAT_EQ(cache.sample(10.0f, 20.0f, 15.0f), cache.at(10, 20, 15));
}

TEST_F(ChunkDensityCacheTest, TrilinearMidpointIsAverage) {
    // Set two adjacent values along X: at(5,5,5)=0.0, at(6,5,5)=1.0
    // All other values in the 8-corner neighborhood are 0.0 by default,
    // so we need at(5,5,5)=0.0 and at(6,5,5)=1.0 with all Y+1 and Z+1
    // corners also set up properly for a clean midpoint test.

    // Set a simple linear ramp along X in the cache region
    for (int lz = 0; lz < K_CACHE_SIZE; ++lz)
        for (int ly = 0; ly < K_CACHE_SIZE; ++ly)
            for (int lx = 0; lx < K_CACHE_SIZE; ++lx) {
                int wx = lx - 1;
                int wy = ly - 1;
                int wz = lz - 1;
                density.set(wx, wy, wz, static_cast<float>(lx));
            }

    cache.build(0, 0, 0, density);

    // Midpoint between cache[5] and cache[6] along X should be 5.5
    float mid = cache.sample(5.5f, 5.0f, 5.0f);
    EXPECT_NEAR(mid, 5.5f, 1e-5f);
}

TEST_F(ChunkDensityCacheTest, MaterialCacheNearestSample) {
    ChunkedGrid<uint16_t, 32> materialGrid;
    ChunkMaterialCache matCache;

    // Place a known material at world (1, 1, 2) -> cache (2, 2, 3)
    materialGrid.set(1, 1, 2, 42);
    // Place another at world (1, 1, 3) -> cache (2, 2, 4)
    materialGrid.set(1, 1, 3, 99);

    matCache.build(0, 0, 0, materialGrid);

    // sampleNearest(1.7, 2.3, 3.6) rounds to (2, 2, 4) -> world(1,1,3) = 99
    EXPECT_EQ(matCache.sampleNearest(1.7f, 2.3f, 3.6f), 99);
    // sampleNearest(1.7, 2.3, 3.4) rounds to (2, 2, 3) -> world(1,1,2) = 42
    EXPECT_EQ(matCache.sampleNearest(1.7f, 2.3f, 3.4f), 42);
}
