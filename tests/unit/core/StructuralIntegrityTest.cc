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

TEST_F(StructuralIntegrityTest, CrossChunkPillarSupportingBeamProducesNoDebris) {
    si.setPerFrameBudgetMs(100.0f);

    int eventCount = 0;
    si.setDebrisCallback([&](const DebrisEvent&) { ++eventCount; });

    // Pillar in chunk (0,0,0): x=31, y=0..4
    for (int y = 0; y <= 4; ++y) {
        grid.set(31, y, 0, 1.0f);
    }

    // Beam in chunk (1,0,0): x=32..34, y=4
    for (int x = 32; x <= 34; ++x) {
        grid.set(x, 4, 0, 1.0f);
    }

    si.update(grid, 0.016f);

    EXPECT_EQ(eventCount, 0);
}

TEST_F(StructuralIntegrityTest, UnsupportedCrossChunkBeamProducesDebris) {
    si.setPerFrameBudgetMs(100.0f);

    std::vector<DebrisEvent> events;
    si.setDebrisCallback([&](const DebrisEvent& e) { events.push_back(e); });

    // Beam at y=4 spanning chunk boundary, no ground connection
    for (int x = 32; x <= 34; ++x) {
        grid.set(x, 4, 0, 1.0f);
    }

    si.update(grid, 0.016f);

    EXPECT_EQ(static_cast<int>(events.size()), 3);
}
