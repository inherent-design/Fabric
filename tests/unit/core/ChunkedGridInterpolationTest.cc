#include "fabric/world/ChunkedGrid.hh"
#include <gtest/gtest.h>

using namespace fabric;

class ChunkedGridInterpolationTest : public ::testing::Test {
  protected:
    ChunkedGrid<float> grid;
};

TEST_F(ChunkedGridInterpolationTest, IntegerCoordinatesReturnExactValues) {
    grid.set(3, 4, 5, 7.0f);
    grid.set(10, 20, 30, 42.0f);
    grid.set(0, 0, 0, 1.5f);

    EXPECT_FLOAT_EQ(grid.sampleLinear(3.0f, 4.0f, 5.0f), 7.0f);
    EXPECT_FLOAT_EQ(grid.sampleLinear(10.0f, 20.0f, 30.0f), 42.0f);
    EXPECT_FLOAT_EQ(grid.sampleLinear(0.0f, 0.0f, 0.0f), 1.5f);
}

TEST_F(ChunkedGridInterpolationTest, MidpointAlongX) {
    grid.set(0, 0, 0, 0.0f);
    grid.set(1, 0, 0, 1.0f);

    EXPECT_FLOAT_EQ(grid.sampleLinear(0.5f, 0.0f, 0.0f), 0.5f);
}

TEST_F(ChunkedGridInterpolationTest, MidpointAlongY) {
    grid.set(0, 0, 0, 0.0f);
    grid.set(0, 1, 0, 1.0f);

    EXPECT_FLOAT_EQ(grid.sampleLinear(0.0f, 0.5f, 0.0f), 0.5f);
}

TEST_F(ChunkedGridInterpolationTest, MidpointAlongZ) {
    grid.set(0, 0, 0, 0.0f);
    grid.set(0, 0, 1, 1.0f);

    EXPECT_FLOAT_EQ(grid.sampleLinear(0.0f, 0.0f, 0.5f), 0.5f);
}

TEST_F(ChunkedGridInterpolationTest, CenterOfUnitCubeReturnsAverage) {
    // Set all 8 corners of a unit cube
    grid.set(0, 0, 0, 0.0f);
    grid.set(1, 0, 0, 2.0f);
    grid.set(0, 1, 0, 4.0f);
    grid.set(1, 1, 0, 6.0f);
    grid.set(0, 0, 1, 8.0f);
    grid.set(1, 0, 1, 10.0f);
    grid.set(0, 1, 1, 12.0f);
    grid.set(1, 1, 1, 14.0f);

    // At center (0.5, 0.5, 0.5), trilinear interpolation of all 8 corners
    // gives the arithmetic mean: (0+2+4+6+8+10+12+14) / 8 = 7.0
    EXPECT_FLOAT_EQ(grid.sampleLinear(0.5f, 0.5f, 0.5f), 7.0f);
}

TEST_F(ChunkedGridInterpolationTest, CrossChunkBoundaryInterpolation) {
    // Chunk boundary is at 32. Positions 31 and 32 are in different chunks.
    grid.set(31, 0, 0, 10.0f);
    grid.set(32, 0, 0, 20.0f);

    // Midpoint between chunk 0 and chunk 1
    EXPECT_FLOAT_EQ(grid.sampleLinear(31.5f, 0.0f, 0.0f), 15.0f);

    // Quarter points
    EXPECT_FLOAT_EQ(grid.sampleLinear(31.25f, 0.0f, 0.0f), 12.5f);
    EXPECT_FLOAT_EQ(grid.sampleLinear(31.75f, 0.0f, 0.0f), 17.5f);
}

TEST_F(ChunkedGridInterpolationTest, DefaultValuesAtUnsetPositions) {
    // No values set -- sampleLinear should return default T{} = 0.0f
    EXPECT_FLOAT_EQ(grid.sampleLinear(0.5f, 0.5f, 0.5f), 0.0f);
    EXPECT_FLOAT_EQ(grid.sampleLinear(100.3f, 200.7f, 300.1f), 0.0f);
}

TEST_F(ChunkedGridInterpolationTest, NonUniformCornerValues) {
    // Verify interpolation with asymmetric corner values
    grid.set(0, 0, 0, 1.0f);
    grid.set(1, 0, 0, 1.0f);
    grid.set(0, 1, 0, 1.0f);
    grid.set(1, 1, 0, 1.0f);
    grid.set(0, 0, 1, 0.0f);
    grid.set(1, 0, 1, 0.0f);
    grid.set(0, 1, 1, 0.0f);
    grid.set(1, 1, 1, 0.0f);

    // At z=0, all corners are 1.0. At z=1, all corners are 0.0.
    // So sampleLinear at z=0.25 should give 0.75.
    EXPECT_FLOAT_EQ(grid.sampleLinear(0.5f, 0.5f, 0.25f), 0.75f);
    EXPECT_FLOAT_EQ(grid.sampleLinear(0.5f, 0.5f, 0.75f), 0.25f);
}

TEST_F(ChunkedGridInterpolationTest, NegativeCoordinateInterpolation) {
    grid.set(-1, 0, 0, 10.0f);
    grid.set(0, 0, 0, 20.0f);

    EXPECT_FLOAT_EQ(grid.sampleLinear(-0.5f, 0.0f, 0.0f), 15.0f);
}
