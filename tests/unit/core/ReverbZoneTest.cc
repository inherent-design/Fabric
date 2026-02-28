#include "fabric/core/ReverbZone.hh"
#include "fabric/core/ChunkedGrid.hh"

#include <gtest/gtest.h>

using namespace fabric;

// ---------------------------------------------------------------------------
// Helper: build a sealed box of solid voxels with air interior.
// Walls at [0..size-1] boundaries, air inside [1..size-2].
// ---------------------------------------------------------------------------
static ChunkedGrid<float> makeSealedBox(int size) {
    ChunkedGrid<float> grid;
    for (int x = 0; x < size; ++x) {
        for (int y = 0; y < size; ++y) {
            for (int z = 0; z < size; ++z) {
                bool wall = (x == 0 || x == size - 1 || y == 0 || y == size - 1 || z == 0 || z == size - 1);
                grid.set(x, y, z, wall ? 1.0f : 0.0f);
            }
        }
    }
    return grid;
}

// ---------------------------------------------------------------------------
// 1. Sealed box: volume, surface area, openness, completeness
// ---------------------------------------------------------------------------
TEST(ReverbZoneTest, SealedBoxMetrics) {
    // 12x12x12 box: interior is 10x10x10 = 1000 air voxels.
    auto grid = makeSealedBox(12);

    // Start BFS inside the box.
    auto est = estimateZone(grid, 5, 5, 5, 0.5f, 100000);

    EXPECT_EQ(est.volume, 1000);
    EXPECT_EQ(est.surfaceArea, 600); // 6 faces * 10*10
    EXPECT_FLOAT_EQ(est.openness, 0.0f);
    EXPECT_TRUE(est.complete);
}

// ---------------------------------------------------------------------------
// 2. Open area: no walls, BFS expands until budget, high openness
// ---------------------------------------------------------------------------
TEST(ReverbZoneTest, OpenAreaHighOpenness) {
    // Empty grid: all voxels return default T{} = 0.0f (air).
    ChunkedGrid<float> grid;

    // Budget-limited so BFS can't go forever.
    auto est = estimateZone(grid, 0, 0, 0, 0.5f, 500);

    EXPECT_GT(est.volume, 0);
    EXPECT_EQ(est.surfaceArea, 0); // No solid neighbors anywhere.
    EXPECT_GT(est.openness, 0.8f);
    EXPECT_FALSE(est.complete);
}

// ---------------------------------------------------------------------------
// 3. Budget cap: small budget, verify partial result
// ---------------------------------------------------------------------------
TEST(ReverbZoneTest, BudgetCapPartialResult) {
    auto grid = makeSealedBox(12);

    // Budget smaller than interior volume (1000).
    auto est = estimateZone(grid, 5, 5, 5, 0.5f, 100);

    EXPECT_EQ(est.volume, 100);
    EXPECT_FALSE(est.complete);
}

// ---------------------------------------------------------------------------
// 4. Cache invalidation: reset() clears state
// ---------------------------------------------------------------------------
TEST(ReverbZoneTest, ResetClearsState) {
    auto grid = makeSealedBox(12);

    ReverbZoneEstimator estimator;
    estimator.reset(5, 5, 5);
    estimator.advanceBFS(grid, 0.5f, 50);

    auto partial = estimator.estimate();
    EXPECT_EQ(partial.volume, 50);

    // Reset to same position: state should be fresh.
    estimator.reset(5, 5, 5);
    EXPECT_FALSE(estimator.isComplete());

    auto afterReset = estimator.estimate();
    EXPECT_EQ(afterReset.volume, 0);
}

// ---------------------------------------------------------------------------
// 5. Parameter mapping: sealed box RT60 via Sabine, open area low wetMix
// ---------------------------------------------------------------------------
TEST(ReverbZoneTest, SealedBoxReverbParams) {
    ZoneEstimate sealed;
    sealed.volume = 1000;
    sealed.surfaceArea = 600;
    sealed.openness = 0.0f;
    sealed.complete = true;

    auto params = mapToReverbParams(sealed, 1.0f);

    // Sabine: RT60 = 0.161 * 1000 / (0.3 * 600) = 161 / 180 ≈ 0.894
    EXPECT_NEAR(params.decayTime, 0.894f, 0.01f);
    EXPECT_GE(params.decayTime, 0.1f);
    EXPECT_LE(params.decayTime, 3.0f);
    EXPECT_GT(params.damping, 0.1f);
    EXPECT_GT(params.wetMix, 0.0f);
}

TEST(ReverbZoneTest, OpenAreaLowWetMix) {
    ZoneEstimate open;
    open.volume = 500;
    open.surfaceArea = 0;
    open.openness = 0.95f;
    open.complete = false;

    auto params = mapToReverbParams(open);

    // No surface area: RT60 clamped to minimum 0.1.
    EXPECT_FLOAT_EQ(params.decayTime, 0.1f);
    // wetMix should be low due to high openness.
    EXPECT_LT(params.wetMix, 0.1f);
}

// ---------------------------------------------------------------------------
// 6. Empty grid (all air): high openness
// ---------------------------------------------------------------------------
TEST(ReverbZoneTest, EmptyGridAllAir) {
    ChunkedGrid<float> grid;

    auto est = estimateZone(grid, 0, 0, 0, 0.5f, 1000);

    EXPECT_GT(est.volume, 0);
    EXPECT_EQ(est.surfaceArea, 0);
    EXPECT_GT(est.openness, 0.8f);
    EXPECT_FALSE(est.complete);
}

// ---------------------------------------------------------------------------
// 7. Single voxel start in solid: volume = 0
// ---------------------------------------------------------------------------
TEST(ReverbZoneTest, StartInSolidZeroVolume) {
    ChunkedGrid<float> grid;
    grid.set(5, 5, 5, 1.0f); // Solid at start.

    // Surround with solid so BFS can't escape.
    for (const auto& off :
         std::vector<std::array<int, 3>>{{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}}) {
        grid.set(5 + off[0], 5 + off[1], 5 + off[2], 1.0f);
    }

    auto est = estimateZone(grid, 5, 5, 5, 0.5f, 1000);

    EXPECT_EQ(est.volume, 0);
}

// ---------------------------------------------------------------------------
// 8. Incremental convergence: advanceBFS multiple times = one-shot
// ---------------------------------------------------------------------------
TEST(ReverbZoneTest, IncrementalConvergence) {
    auto grid = makeSealedBox(12);

    // One-shot.
    auto oneShot = estimateZone(grid, 5, 5, 5, 0.5f, 100000);

    // Incremental: many small budgets.
    ReverbZoneEstimator estimator;
    estimator.reset(5, 5, 5);
    while (!estimator.isComplete()) {
        estimator.advanceBFS(grid, 0.5f, 50);
    }
    auto incremental = estimator.estimate();

    EXPECT_EQ(incremental.volume, oneShot.volume);
    EXPECT_EQ(incremental.surfaceArea, oneShot.surfaceArea);
    EXPECT_FLOAT_EQ(incremental.openness, oneShot.openness);
    EXPECT_EQ(incremental.complete, oneShot.complete);
}

// ---------------------------------------------------------------------------
// 9. Zero-volume zone produces safe reverb params (no division by zero)
// ---------------------------------------------------------------------------
TEST(ReverbZoneTest, ZeroVolumeParamsSafe) {
    ZoneEstimate empty;
    empty.volume = 0;
    empty.surfaceArea = 0;
    empty.openness = 0.0f;
    empty.complete = true;

    auto params = mapToReverbParams(empty);

    EXPECT_FLOAT_EQ(params.decayTime, 0.1f);
    EXPECT_FLOAT_EQ(params.damping, 0.9f);
    EXPECT_FLOAT_EQ(params.wetMix, 0.0f);
}

// ---------------------------------------------------------------------------
// 10. RT60 clamps to [0.1, 3.0] for extreme volumes
// ---------------------------------------------------------------------------
TEST(ReverbZoneTest, RT60ClampRange) {
    // Large volume, tiny surface: would give huge RT60 unclamped.
    ZoneEstimate huge;
    huge.volume = 1000000;
    huge.surfaceArea = 6;
    huge.openness = 0.0f;
    huge.complete = true;

    auto params = mapToReverbParams(huge);
    EXPECT_FLOAT_EQ(params.decayTime, 3.0f);

    // Tiny volume, large surface: would give tiny RT60.
    ZoneEstimate tiny;
    tiny.volume = 1;
    tiny.surfaceArea = 6;
    tiny.openness = 0.0f;
    tiny.complete = true;

    auto paramsSmall = mapToReverbParams(tiny);
    EXPECT_GE(paramsSmall.decayTime, 0.1f);
    EXPECT_LE(paramsSmall.decayTime, 3.0f);
}
