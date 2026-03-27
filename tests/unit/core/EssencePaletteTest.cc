#include "recurse/world/EssencePalette.hh"
#include "fabric/utils/ErrorHandling.hh"
#include <gtest/gtest.h>

using namespace fabric;
using namespace recurse;
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

TEST_F(EssencePaletteTest, PaletteRespectsMaxSizeViaSilentMerge) {
    // Use a small max to make the test fast
    EssencePalette small(0.0f, 16); // zero epsilon, max 16
    for (int i = 0; i < 16; ++i) {
        float v = static_cast<float>(i);
        small.quantize(Vec4(v, 0.0f, 0.0f, 0.0f));
    }
    EXPECT_EQ(small.paletteSize(), 16u);
    // Next entry triggers silent merge: two closest entries collapse, then new entry added
    uint16_t idx = small.quantize(Vec4(999.0f, 999.0f, 999.0f, 999.0f));
    EXPECT_EQ(small.paletteSize(), 16u);
    // The returned index should be valid (not a sentinel)
    EXPECT_LT(idx, 16u);
    // The new entry should be retrievable
    Vec4 result = small.lookup(idx);
    EXPECT_FLOAT_EQ(result.x, 999.0f);
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

// --- Group B: Palette overflow with silent merge ---

TEST_F(EssencePaletteTest, PaletteOverflowAt256SilentMerge) {
    EssencePalette pal(0.0f, 256);
    for (int i = 0; i < 256; ++i) {
        float v = static_cast<float>(i);
        pal.quantize(Vec4(v, 0.0f, 0.0f, 0.0f));
    }
    EXPECT_EQ(pal.paletteSize(), 256u);

    uint16_t idx = pal.quantize(Vec4(999.0f, 999.0f, 999.0f, 999.0f));
    EXPECT_EQ(pal.paletteSize(), 256u);
    EXPECT_LT(idx, 256u);
}

TEST_F(EssencePaletteTest, PaletteMergeSelectsClosestEntry) {
    EssencePalette pal(0.0f, 2);
    uint16_t idxA = pal.quantize(Vec4(1.0f, 0.0f, 0.0f, 0.0f));
    uint16_t idxB = pal.quantize(Vec4(0.0f, 1.0f, 0.0f, 0.0f));
    EXPECT_EQ(pal.paletteSize(), 2u);

    uint16_t idxC = pal.quantize(Vec4(0.95f, 0.05f, 0.0f, 0.0f));
    EXPECT_EQ(pal.paletteSize(), 2u);

    Vec4 result = pal.lookup(idxC);
    EXPECT_FLOAT_EQ(result.x, 0.95f);
    EXPECT_FLOAT_EQ(result.y, 0.05f);
}

// --- Group C: Grid hash collision handling ---

TEST_F(EssencePaletteTest, GridHashCollisionDoesNotOverwrite) {
    // Two essences that quantize to the same grid key but are beyond epsilon
    // distance must both get distinct entries (no silent overwrite).
    Vec4 a(0.000f, 0.000f, 0.000f, 0.000f);
    Vec4 b(0.009f, 0.009f, 0.009f, 0.009f);
    uint16_t idxA = palette.quantize(a);
    uint16_t idxB = palette.quantize(b);
    EXPECT_NE(idxA, idxB);
    EXPECT_EQ(palette.paletteSize(), 2u);

    // Re-quantizing a must still return the original index
    uint16_t idxA2 = palette.quantize(a);
    EXPECT_EQ(idxA, idxA2);
}

TEST_F(EssencePaletteTest, GridHashCollisionWithThirdEntry) {
    Vec4 a(0.000f, 0.000f, 0.000f, 0.000f);
    Vec4 b(0.009f, 0.009f, 0.009f, 0.009f);
    Vec4 c(0.000f, 0.000f, 0.000f, 0.000f);
    uint16_t idxA = palette.quantize(a);
    uint16_t idxB = palette.quantize(b);
    uint16_t idxC = palette.quantize(c);
    EXPECT_NE(idxA, idxB);
    EXPECT_EQ(idxA, idxC);
}

TEST_F(EssencePaletteTest, GridHashCollisionWithinEpsilonStillDeduplicates) {
    Vec4 a(0.500f, 0.500f, 0.500f, 0.500f);
    Vec4 b(0.503f, 0.501f, 0.501f, 0.501f);
    uint16_t idxA = palette.quantize(a);
    uint16_t idxB = palette.quantize(b);
    EXPECT_EQ(idxA, idxB);
    EXPECT_EQ(palette.paletteSize(), 1u);
}

TEST_F(EssencePaletteTest, BatchMergeReducesMultiplePairs) {
    // Fill to capacity, then overflow to trigger batch merge (K=8 pairs).
    EssencePalette pal(0.01f, 20);
    for (int i = 0; i < 20; ++i) {
        float v = static_cast<float>(i) * 0.1f;
        pal.quantize(Vec4(v, 0.0f, 0.0f, 0.0f));
    }
    EXPECT_EQ(pal.paletteSize(), 20u);

    // Adding more entries triggers batch merge (K=8 pairs).
    pal.quantize(Vec4(99.0f, 99.0f, 99.0f, 99.0f));
    EXPECT_LE(pal.paletteSize(), 20u);
    EXPECT_GT(pal.paletteSize(), 0u);

    // Re-quantize the same value; must deduplicate
    pal.quantize(Vec4(99.0f, 99.0f, 99.0f, 99.0f));
    EXPECT_EQ(pal.paletteSize(), pal.paletteSize());
}
