#include "fabric/core/EssencePalette.hh"
#include "fabric/utils/ErrorHandling.hh"
#include <gtest/gtest.h>

using namespace fabric;
using Vec4 = Vector4<float, Space::World>;

class EssencePaletteTest : public ::testing::Test {
  protected:
    EssencePalette palette{0.01f};
};

TEST_F(EssencePaletteTest, QuantizeAndLookupRoundTrip) {
    Vec4 essence(0.5f, 0.3f, 0.8f, 1.0f);
    uint16_t idx = palette.quantize(essence);
    Vec4 result = palette.lookup(idx);
    EXPECT_FLOAT_EQ(result.x, essence.x);
    EXPECT_FLOAT_EQ(result.y, essence.y);
    EXPECT_FLOAT_EQ(result.z, essence.z);
    EXPECT_FLOAT_EQ(result.w, essence.w);
}

TEST_F(EssencePaletteTest, DuplicateEssenceMapsToSameIndex) {
    Vec4 a(0.5f, 0.3f, 0.8f, 1.0f);
    Vec4 b(0.5f, 0.3f, 0.8f, 1.0f);
    uint16_t idxA = palette.quantize(a);
    uint16_t idxB = palette.quantize(b);
    EXPECT_EQ(idxA, idxB);
    EXPECT_EQ(palette.paletteSize(), 1u);
}

TEST_F(EssencePaletteTest, PaletteGrowsForUniqueEssences) {
    palette.quantize(Vec4(0.0f, 0.0f, 0.0f, 0.0f));
    palette.quantize(Vec4(1.0f, 1.0f, 1.0f, 1.0f));
    palette.quantize(Vec4(0.5f, 0.5f, 0.5f, 0.5f));
    EXPECT_EQ(palette.paletteSize(), 3u);
}

TEST_F(EssencePaletteTest, EpsilonDeduplication) {
    Vec4 a(0.500f, 0.300f, 0.800f, 1.000f);
    // Within epsilon (distance = 0.005 < 0.01)
    Vec4 b(0.503f, 0.301f, 0.801f, 1.001f);
    uint16_t idxA = palette.quantize(a);
    uint16_t idxB = palette.quantize(b);
    EXPECT_EQ(idxA, idxB);
    EXPECT_EQ(palette.paletteSize(), 1u);
}

TEST_F(EssencePaletteTest, EpsilonBoundaryDistinguishes) {
    Vec4 a(0.0f, 0.0f, 0.0f, 0.0f);
    // Distance = sqrt(4 * 0.1^2) = 0.2, well above epsilon 0.01
    Vec4 b(0.1f, 0.1f, 0.1f, 0.1f);
    uint16_t idxA = palette.quantize(a);
    uint16_t idxB = palette.quantize(b);
    EXPECT_NE(idxA, idxB);
    EXPECT_EQ(palette.paletteSize(), 2u);
}

TEST_F(EssencePaletteTest, PaletteRespectsMaxSize) {
    EssencePalette small(0.0f); // zero epsilon = no dedup
    for (int i = 0; i < static_cast<int>(EssencePalette::kMaxPaletteSize); ++i) {
        float v = static_cast<float>(i) / 100000.0f;
        small.quantize(Vec4(v, 0.0f, 0.0f, 0.0f));
    }
    EXPECT_EQ(small.paletteSize(), EssencePalette::kMaxPaletteSize);
    // Next entry should return overflow sentinel
    uint16_t overflow = small.quantize(Vec4(999.0f, 999.0f, 999.0f, 999.0f));
    EXPECT_EQ(overflow, EssencePalette::kMaxPaletteSize);
    EXPECT_EQ(small.paletteSize(), EssencePalette::kMaxPaletteSize);
}

TEST_F(EssencePaletteTest, LookupOutOfRangeThrows) {
    EXPECT_THROW(palette.lookup(0), FabricException);
    palette.quantize(Vec4(1.0f, 0.0f, 0.0f, 0.0f));
    EXPECT_THROW(palette.lookup(1), FabricException);
    EXPECT_NO_THROW(palette.lookup(0));
}

TEST_F(EssencePaletteTest, ClearResetsPalette) {
    palette.quantize(Vec4(1.0f, 0.0f, 0.0f, 0.0f));
    palette.quantize(Vec4(0.0f, 1.0f, 0.0f, 0.0f));
    EXPECT_EQ(palette.paletteSize(), 2u);
    palette.clear();
    EXPECT_EQ(palette.paletteSize(), 0u);
}

TEST_F(EssencePaletteTest, AddEntryDeduplicates) {
    Vec4 essence(0.5f, 0.5f, 0.5f, 0.5f);
    uint16_t idx1 = palette.addEntry(essence);
    uint16_t idx2 = palette.addEntry(essence);
    EXPECT_EQ(idx1, idx2);
    EXPECT_EQ(palette.paletteSize(), 1u);
}

TEST_F(EssencePaletteTest, SetEpsilonChangesTolerance) {
    palette.setEpsilon(1.0f);
    Vec4 a(0.0f, 0.0f, 0.0f, 0.0f);
    Vec4 b(0.3f, 0.3f, 0.3f, 0.3f); // distance = 0.6, within epsilon 1.0
    palette.quantize(a);
    palette.quantize(b);
    EXPECT_EQ(palette.paletteSize(), 1u);
}

TEST_F(EssencePaletteTest, ZeroEpsilonRequiresExactMatch) {
    EssencePalette exact(0.0f);
    Vec4 a(0.5f, 0.5f, 0.5f, 0.5f);
    Vec4 b(0.5f, 0.5f, 0.5f, 0.500001f);
    exact.quantize(a);
    exact.quantize(b);
    EXPECT_EQ(exact.paletteSize(), 2u);
}
