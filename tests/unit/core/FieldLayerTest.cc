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
