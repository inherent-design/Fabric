#include "fabric/core/StructuralIntegrity.hh"
#include "fabric/core/ChunkedGrid.hh"

#include <gtest/gtest.h>

using namespace fabric;

class StructuralIntegrityTest : public ::testing::Test {
  protected:
    StructuralIntegrity si;
    ChunkedGrid<float> grid;

    void TearDown() override { si.setDebrisCallback(nullptr); }
};

TEST_F(StructuralIntegrityTest, DefaultBudgetIs1ms) {
    EXPECT_FLOAT_EQ(si.getPerFrameBudgetMs(), 1.0f);
}

TEST_F(StructuralIntegrityTest, SetAndGetPerFrameBudget) {
    si.setPerFrameBudgetMs(5.0f);
    EXPECT_FLOAT_EQ(si.getPerFrameBudgetMs(), 5.0f);

    si.setPerFrameBudgetMs(0.0f);
    EXPECT_FLOAT_EQ(si.getPerFrameBudgetMs(), 0.0f);
}

TEST_F(StructuralIntegrityTest, NoCallbackNoCrash) {
    grid.set(0, 2, 0, 1.0f);
    si.setDebrisCallback(nullptr);

    si.update(grid, 0.016f);

    SUCCEED();
}

TEST_F(StructuralIntegrityTest, ZeroBudgetSkipsProcessing) {
    si.setPerFrameBudgetMs(0.0f);

    int eventCount = 0;
    si.setDebrisCallback([&](const DebrisEvent&) { ++eventCount; });

    grid.set(0, 2, 0, 1.0f);
    si.update(grid, 0.016f);

    EXPECT_EQ(eventCount, 0);
}

TEST_F(StructuralIntegrityTest, FloatingVoxelGeneratesDebris) {
    int eventCount = 0;
    si.setDebrisCallback([&](const DebrisEvent&) { ++eventCount; });

    grid.set(0, 2, 0, 1.0f);
    si.update(grid, 0.016f);

    EXPECT_EQ(eventCount, 1);
}

TEST_F(StructuralIntegrityTest, GroundConnectedStructureProducesNoDebris) {
    int eventCount = 0;
    si.setDebrisCallback([&](const DebrisEvent&) { ++eventCount; });

    grid.set(0, 0, 0, 1.0f);
    grid.set(0, 1, 0, 1.0f);
    grid.set(0, 2, 0, 1.0f);

    si.update(grid, 0.016f);

    EXPECT_EQ(eventCount, 0);
}

TEST_F(StructuralIntegrityTest, MixedGroundedAndFloatingOnlyReportsFloating) {
    int eventCount = 0;
    si.setDebrisCallback([&](const DebrisEvent&) { ++eventCount; });

    grid.set(0, 0, 0, 1.0f);
    grid.set(0, 1, 0, 1.0f);
    grid.set(10, 5, 3, 1.0f);

    si.update(grid, 0.016f);

    EXPECT_EQ(eventCount, 1);
}

TEST_F(StructuralIntegrityTest, CrossChunkGroundConnectionProducesNoDebris) {
    int eventCount = 0;
    si.setDebrisCallback([&](const DebrisEvent&) { ++eventCount; });

    grid.set(31, 0, 0, 1.0f);
    grid.set(32, 0, 0, 1.0f);
    grid.set(32, 1, 0, 1.0f);

    si.update(grid, 0.016f);

    EXPECT_EQ(eventCount, 0);
}

TEST_F(StructuralIntegrityTest, CrossChunkFloatingClusterProducesDebris) {
    si.setPerFrameBudgetMs(100.0f);

    int eventCount = 0;
    si.setDebrisCallback([&](const DebrisEvent&) { ++eventCount; });

    grid.set(31, 5, 0, 1.0f);
    grid.set(32, 5, 0, 1.0f);

    si.update(grid, 0.016f);

    EXPECT_EQ(eventCount, 2);
}

TEST_F(StructuralIntegrityTest, DensityThresholdFiltersLowDensity) {
    int eventCount = 0;
    si.setDebrisCallback([&](const DebrisEvent&) { ++eventCount; });

    grid.set(0, 2, 0, 1.0f);
    grid.set(1, 2, 0, 0.3f);

    si.update(grid, 0.016f);

    EXPECT_EQ(eventCount, 1);
}

TEST_F(StructuralIntegrityTest, TinyBudgetPausesBFSAndResumesAcrossFrames) {
    // Fill a dense 32x32x32 chunk at y=0 (all grounded). The BFS must visit all
    // voxels to confirm they're supported. With a near-zero budget, a single
    // update() call cannot complete the BFS -- it must be spread across frames.

    constexpr int kSize = kStructuralIntegrityChunkSize;
    for (int z = 0; z < kSize; ++z) {
        for (int y = 0; y < kSize; ++y) {
            for (int x = 0; x < kSize; ++x) {
                grid.set(x, y, z, 1.0f);
            }
        }
    }

    // Also place a single floating voxel in a second chunk so we can verify
    // eventual correctness: after all BFS work completes, it should be debris.
    grid.set(0, kSize + 5, 0, 1.0f);

    int debrisCount = 0;
    si.setDebrisCallback([&](const DebrisEvent&) { ++debrisCount; });

    // 0.001 ms = 1000 ns. Too small for a full 32^3 BFS.
    si.setPerFrameBudgetMs(0.001f);

    const int64_t chunkKey = StructuralIntegrity::packKey(0, 0, 0);

    // Run many update() calls. Eventually the BFS should complete.
    bool sawPartialState = false;
    constexpr int kMaxFrames = 100'000;
    int framesUsed = 0;

    for (int i = 0; i < kMaxFrames; ++i) {
        si.update(grid, 0.016f);
        ++framesUsed;

        const auto* partial = si.getPartialState(chunkKey);
        if (partial != nullptr) {
            sawPartialState = true;
        }

        // Once debris is detected, both chunks have been processed
        if (debrisCount > 0) {
            break;
        }
    }

    // The budget was small enough that BFS should have been interrupted at least once
    EXPECT_TRUE(sawPartialState) << "BFS was never interrupted despite tiny budget";

    // Eventually the floating voxel should be reported as debris
    EXPECT_GE(debrisCount, 1) << "Floating voxel was never detected after " << framesUsed << " frames";

    // After completion, partial state should be cleared
    EXPECT_EQ(si.getPartialState(chunkKey), nullptr);
}

TEST_F(StructuralIntegrityTest, ProcessedCellsTracksWorkDone) {
    // Create a small set of grounded voxels. Run BFS to completion with generous
    // budget. Verify processedCells equals the number of BFS pops (which equals
    // the number of supported voxels reachable from ground seeds).

    // 10 grounded voxels in a column at x=0, z=0
    constexpr int kColumnHeight = 10;
    for (int y = 0; y < kColumnHeight; ++y) {
        grid.set(0, y, 0, 1.0f);
    }

    si.setDebrisCallback([](const DebrisEvent&) {});
    si.setPerFrameBudgetMs(1000.0f); // generous

    // Directly invoke floodFillChunk to inspect the state
    StructuralIntegrity::FloodFillState state;
    // Use a large budget so it completes in one call
    bool complete = si.floodFillChunk(0, 0, 0, grid, state, 1'000'000'000LL);

    EXPECT_TRUE(complete);
    // The ground seed at y=0 starts in the queue. BFS pops it and discovers y=1,
    // pops y=1 and discovers y=2, etc. Each pop increments processedCells.
    // Total pops = number of supported voxels = kColumnHeight.
    EXPECT_EQ(state.processedCells, kColumnHeight);
    EXPECT_TRUE(state.disconnectedVoxels.empty());
}
