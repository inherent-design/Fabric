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

TEST_F(StructuralIntegrityTest, CrossChunkPillarSupportsBeamNoDebris) {
    si.setPerFrameBudgetMs(100.0f);

    int eventCount = 0;
    si.setDebrisCallback([&](const DebrisEvent&) { ++eventCount; });

    // Pillar at x=31, y=0..4 in chunk (0,0,0)
    for (int y = 0; y <= 4; ++y) {
        grid.set(31, y, 0, 1.0f);
    }
    // Beam at x=32..34, y=4 in chunk (1,0,0)
    for (int x = 32; x <= 34; ++x) {
        grid.set(x, 4, 0, 1.0f);
    }

    si.update(grid, 0.016f);

    EXPECT_EQ(eventCount, 0);
}

TEST_F(StructuralIntegrityTest, UnsupportedBeamProducesDebrisForAllVoxels) {
    si.setPerFrameBudgetMs(100.0f);

    int eventCount = 0;
    si.setDebrisCallback([&](const DebrisEvent&) { ++eventCount; });

    // Beam at x=32..34, y=4 without any pillar support
    for (int x = 32; x <= 34; ++x) {
        grid.set(x, 4, 0, 1.0f);
    }

    si.update(grid, 0.016f);

    EXPECT_EQ(eventCount, 3);
}

TEST_F(StructuralIntegrityTest, DensityThresholdFiltersLowDensity) {
    int eventCount = 0;
    si.setDebrisCallback([&](const DebrisEvent&) { ++eventCount; });

    grid.set(0, 2, 0, 1.0f);
    grid.set(1, 2, 0, 0.3f);

    si.update(grid, 0.016f);

    EXPECT_EQ(eventCount, 1);
}

TEST_F(StructuralIntegrityTest, BudgetLimitsBFSAcrossFrames) {
    si.setPerFrameBudgetMs(0.001f);

    // Dense 32x32x32 chunk on the ground
    for (int z = 0; z < 32; ++z) {
        for (int y = 0; y < 32; ++y) {
            for (int x = 0; x < 32; ++x) {
                grid.set(x, y, z, 1.0f);
            }
        }
    }

    // Floating cluster far from ground-connected voxels
    grid.set(100, 50, 100, 1.0f);
    grid.set(101, 50, 100, 1.0f);

    int debrisCount = 0;
    si.setDebrisCallback([&](const DebrisEvent&) { ++debrisCount; });

    int iterations = 0;
    constexpr int kMaxIterations = 100000;
    while (debrisCount == 0 && iterations < kMaxIterations) {
        si.update(grid, 0.016f);
        ++iterations;
    }

    // With 1us budget and 32K+ voxels, BFS must span multiple frames
    EXPECT_GT(iterations, 1) << "Budget should split BFS across multiple frames";

    // Floating cluster detected as debris
    EXPECT_EQ(debrisCount, 2);
}

TEST_F(StructuralIntegrityTest, ProcessedCellsIncrements) {
    si.setPerFrameBudgetMs(0.001f);

    // Dense 32x32x32 chunk on the ground
    for (int z = 0; z < 32; ++z) {
        for (int y = 0; y < 32; ++y) {
            for (int x = 0; x < 32; ++x) {
                grid.set(x, y, z, 1.0f);
            }
        }
    }

    si.setDebrisCallback([](const DebrisEvent&) {});
    EXPECT_EQ(si.getProcessedCells(), 0u);

    si.update(grid, 0.016f);
    const uint64_t afterFirst = si.getProcessedCells();
    EXPECT_GT(afterFirst, 0u) << "processedCells should increment after first update";

    si.update(grid, 0.016f);
    const uint64_t afterSecond = si.getProcessedCells();
    EXPECT_GT(afterSecond, afterFirst) << "processedCells should continue incrementing";
}

TEST_F(StructuralIntegrityTest, GetProcessedCellsDefaultsToZero) {
    EXPECT_EQ(si.getProcessedCells(), 0u);
}
