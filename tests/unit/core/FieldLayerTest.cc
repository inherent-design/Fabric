#include "fabric/core/FieldLayer.hh"
#include <cmath>
#include <gtest/gtest.h>

using namespace fabric;

class FieldLayerTest : public ::testing::Test {};

TEST_F(FieldLayerTest, ReadWriteRoundtrip) {
    DensityField field;
    field.write(1, 2, 3, 5.0f);
    EXPECT_FLOAT_EQ(field.read(1, 2, 3), 5.0f);
}

TEST_F(FieldLayerTest, FillRegion) {
    DensityField field;
    field.fill(0, 0, 0, 3, 3, 3, 7.5f);

    for (int z = 0; z <= 3; ++z)
        for (int y = 0; y <= 3; ++y)
            for (int x = 0; x <= 3; ++x)
                EXPECT_FLOAT_EQ(field.read(x, y, z), 7.5f);

    // Outside the region should be default
    EXPECT_FLOAT_EQ(field.read(4, 0, 0), 0.0f);
}

TEST_F(FieldLayerTest, SampleRadius0) {
    DensityField field;
    field.write(5, 5, 5, 10.0f);
    EXPECT_FLOAT_EQ(field.sample(5, 5, 5, 0), 10.0f);
}

TEST_F(FieldLayerTest, SampleRadius1Average) {
    DensityField field;
    // Only center cell has a value, rest are 0
    field.write(5, 5, 5, 27.0f);
    // radius 1 = 3x3x3 = 27 cells, sum = 27.0, average = 1.0
    float avg = field.sample(5, 5, 5, 1);
    EXPECT_NEAR(avg, 1.0f, 1e-5f);
}

TEST_F(FieldLayerTest, SampleRadius1AllFilled) {
    DensityField field;
    field.fill(4, 4, 4, 6, 6, 6, 3.0f);
    // All 27 cells are 3.0, average should be 3.0
    float avg = field.sample(5, 5, 5, 1);
    EXPECT_NEAR(avg, 3.0f, 1e-5f);
}

TEST_F(FieldLayerTest, DensityFieldCompiles) {
    DensityField field;
    field.write(0, 0, 0, 1.0f);
    EXPECT_FLOAT_EQ(field.read(0, 0, 0), 1.0f);
}

TEST_F(FieldLayerTest, EssenceFieldCompiles) {
    EssenceField field;
    Vector4<float, Space::World> val(1.0f, 2.0f, 3.0f, 4.0f);
    field.write(0, 0, 0, val);
    auto result = field.read(0, 0, 0);
    EXPECT_FLOAT_EQ(result.x, 1.0f);
    EXPECT_FLOAT_EQ(result.y, 2.0f);
    EXPECT_FLOAT_EQ(result.z, 3.0f);
    EXPECT_FLOAT_EQ(result.w, 4.0f);
}

TEST_F(FieldLayerTest, EssenceFieldSample) {
    EssenceField field;
    Vector4<float, Space::World> val(27.0f, 54.0f, 81.0f, 108.0f);
    field.write(5, 5, 5, val);
    auto avg = field.sample(5, 5, 5, 1); // 27 cells, only one has value
    EXPECT_NEAR(avg.x, 1.0f, 1e-5f);
    EXPECT_NEAR(avg.y, 2.0f, 1e-5f);
    EXPECT_NEAR(avg.z, 3.0f, 1e-5f);
    EXPECT_NEAR(avg.w, 4.0f, 1e-5f);
}

// ============================================================
// Larger grid tests (chunk-sized or bigger)
// ============================================================

TEST_F(FieldLayerTest, LargeGridWriteRead32) {
    DensityField field;
    constexpr int SIZE = 32;

    // Fill a 32x32x32 volume
    for (int z = 0; z < SIZE; ++z)
        for (int y = 0; y < SIZE; ++y)
            for (int x = 0; x < SIZE; ++x)
                field.write(x, y, z, static_cast<float>(x + y + z));

    // Verify a sample of values across the volume
    EXPECT_FLOAT_EQ(field.read(0, 0, 0), 0.0f);
    EXPECT_FLOAT_EQ(field.read(15, 15, 15), 45.0f);
    EXPECT_FLOAT_EQ(field.read(31, 31, 31), 93.0f);
    EXPECT_FLOAT_EQ(field.read(0, 31, 0), 31.0f);
    EXPECT_FLOAT_EQ(field.read(31, 0, 31), 62.0f);
}

TEST_F(FieldLayerTest, LargeGridFillAndSample) {
    DensityField field;
    constexpr int SIZE = 32;

    // Fill entire 32^3 with uniform value
    field.fill(0, 0, 0, SIZE - 1, SIZE - 1, SIZE - 1, 2.5f);

    // Sample at center with radius 1: all neighbors are 2.5, average = 2.5
    float avg = field.sample(16, 16, 16, 1);
    EXPECT_NEAR(avg, 2.5f, 1e-5f);
}

TEST_F(FieldLayerTest, LargeGridNegativeCoords) {
    DensityField field;

    // ChunkedGrid supports negative coordinates
    field.write(-32, -32, -32, 7.0f);
    field.write(31, 31, 31, 8.0f);

    EXPECT_FLOAT_EQ(field.read(-32, -32, -32), 7.0f);
    EXPECT_FLOAT_EQ(field.read(31, 31, 31), 8.0f);
    EXPECT_FLOAT_EQ(field.read(0, 0, 0), 0.0f); // default
}

TEST_F(FieldLayerTest, LargeGridSparseWrite) {
    DensityField field;

    // Write only a few cells in a large space -- tests chunk allocation
    field.write(0, 0, 0, 1.0f);
    field.write(100, 100, 100, 2.0f);
    field.write(-100, -100, -100, 3.0f);

    EXPECT_FLOAT_EQ(field.read(0, 0, 0), 1.0f);
    EXPECT_FLOAT_EQ(field.read(100, 100, 100), 2.0f);
    EXPECT_FLOAT_EQ(field.read(-100, -100, -100), 3.0f);

    // Unwritten regions return default
    EXPECT_FLOAT_EQ(field.read(50, 50, 50), 0.0f);
}

TEST_F(FieldLayerTest, LargeGridEssenceField64) {
    EssenceField field;
    constexpr int SIZE = 16; // 16^3 = 4096 cells for Vector4

    Vector4<float, Space::World> val(1.0f, 2.0f, 3.0f, 4.0f);
    for (int z = 0; z < SIZE; ++z)
        for (int y = 0; y < SIZE; ++y)
            for (int x = 0; x < SIZE; ++x)
                field.write(x, y, z, val);

    // Sample at center: uniform fill means average == the fill value
    auto avg = field.sample(8, 8, 8, 1);
    EXPECT_NEAR(avg.x, 1.0f, 1e-5f);
    EXPECT_NEAR(avg.y, 2.0f, 1e-5f);
    EXPECT_NEAR(avg.z, 3.0f, 1e-5f);
    EXPECT_NEAR(avg.w, 4.0f, 1e-5f);
}
