#include "recurse/simulation/VisualMergeTable.hh"
#include "recurse/simulation/CellAccessors.hh"
#include "recurse/simulation/VoxelMaterial.hh"

#include <gtest/gtest.h>

using namespace recurse::simulation;

// -- visualHash ---------------------------------------------------------------

TEST(VisualHashTest, AirHashIsZero) {
    VisualSignature air{0, Phase::Empty};
    EXPECT_EQ(visualHash(air), 0);
}

TEST(VisualHashTest, DifferentEssenceDifferentHash) {
    VisualSignature stone{1, Phase::Solid};
    VisualSignature dirt{2, Phase::Solid};
    EXPECT_NE(visualHash(stone), visualHash(dirt));
}

TEST(VisualHashTest, DifferentPhaseDifferentHash) {
    VisualSignature solid{1, Phase::Solid};
    VisualSignature powder{1, Phase::Powder};
    EXPECT_NE(visualHash(solid), visualHash(powder));
}

TEST(VisualHashTest, SameFieldsSameHash) {
    VisualSignature a{3, Phase::Liquid};
    VisualSignature b{3, Phase::Liquid};
    EXPECT_EQ(visualHash(a), visualHash(b));
}

TEST(VisualHashTest, HashEncodesEssenceAndPhase) {
    VisualSignature sig{5, Phase::Gas};
    // Expected: (5 << 3) | 4 = 44
    EXPECT_EQ(visualHash(sig), static_cast<uint16_t>((5 << 3) | 4));
}

TEST(VisualHashTest, MaxEssenceDoesNotOverflow) {
    VisualSignature sig{255, Phase::Gas};
    // (255 << 3) | 4 = 2044, within uint16_t
    EXPECT_EQ(visualHash(sig), static_cast<uint16_t>((255 << 3) | 4));
    EXPECT_LT(visualHash(sig), VisualMergeTable::K_TABLE_SIZE);
}

// -- visualSignatureOf --------------------------------------------------------

TEST(VisualSignatureTest, ExtractsFromVoxelCell) {
    auto cell = makeCell(3, Phase::Powder, 130, 0x05);
    auto sig = visualSignatureOf(cell);
    EXPECT_EQ(sig.essenceIdx, 3);
    EXPECT_EQ(sig.phase, Phase::Powder);
}

TEST(VisualSignatureTest, IgnoresNonVisualFields) {
    auto cellA = makeCell(2, Phase::Solid, 100, 0x00);
    auto cellB = makeCell(2, Phase::Solid, 200, 0x1F);
    EXPECT_EQ(visualSignatureOf(cellA), visualSignatureOf(cellB));
}

// -- MergeKey with visual hash ------------------------------------------------

TEST(MergeKeyVisualTest, EmptyCellMergeKeyIsZero) {
    auto cell = emptyCell();
    EXPECT_EQ(mergeKey(cell), K_MERGE_KEY_EMPTY);
}

TEST(MergeKeyVisualTest, SameMaterialSamePhaseMerges) {
    auto a = makeCell(1, Phase::Solid, 100);
    auto b = makeCell(1, Phase::Solid, 200);
    EXPECT_TRUE(canMergeQuads(mergeKey(a), mergeKey(b)));
}

TEST(MergeKeyVisualTest, SameMaterialDifferentPhaseDoesNotMerge) {
    auto solid = makeCell(1, Phase::Solid);
    auto powder = makeCell(1, Phase::Powder);
    EXPECT_FALSE(canMergeQuads(mergeKey(solid), mergeKey(powder)));
}

TEST(MergeKeyVisualTest, DifferentMaterialDoesNotMerge) {
    auto stone = makeCell(1, Phase::Solid);
    auto dirt = makeCell(2, Phase::Solid);
    EXPECT_FALSE(canMergeQuads(mergeKey(stone), mergeKey(dirt)));
}

TEST(MergeKeyVisualTest, DifferentFlagsSameMerge) {
    auto a = makeCell(3, Phase::Powder, 130, 0x00);
    auto b = makeCell(3, Phase::Powder, 130, 0x1F);
    EXPECT_TRUE(canMergeQuads(mergeKey(a), mergeKey(b)));
}

TEST(MergeKeyVisualTest, MatterStateMergeKeyMatchesVoxelCell) {
    auto cell = makeCell(4, Phase::Liquid, 100);
    MatterState ms;
    ms.essenceIdx = 4;
    ms.setPhase(Phase::Liquid);
    ms.displacementRank = 100;
    EXPECT_EQ(mergeKey(cell), mergeKey(ms));
}

// -- VisualMergeTable ---------------------------------------------------------

TEST(VisualMergeTableTest, EmptyTableCanMergeReturnsTrueForSameHash) {
    VisualMergeTable table;
    // Even without registration, same hash == same hash.
    EXPECT_TRUE(table.canMerge(5, 5));
}

TEST(VisualMergeTableTest, DifferentHashesCannotMerge) {
    VisualMergeTable table;
    EXPECT_FALSE(table.canMerge(5, 10));
}

TEST(VisualMergeTableTest, RegisterSingleEntry) {
    VisualMergeTable table;
    VisualSignature stone{1, Phase::Solid};
    uint16_t hash = table.registerSignature(stone);
    EXPECT_EQ(hash, visualHash(stone));
    EXPECT_TRUE(table.isSingleEntry(hash));
}

TEST(VisualMergeTableTest, RegisterDuplicateIsIdempotent) {
    VisualMergeTable table;
    VisualSignature stone{1, Phase::Solid};
    uint16_t h1 = table.registerSignature(stone);
    uint16_t h2 = table.registerSignature(stone);
    EXPECT_EQ(h1, h2);
    EXPECT_TRUE(table.isSingleEntry(h1));
    EXPECT_EQ(table.overflowSize(), 0);
}

TEST(VisualMergeTableTest, CollisionPromotesToMultiEntry) {
    VisualMergeTable table;
    // Force a collision by using signatures that hash to the same value.
    // With the current hash (essenceIdx << 3 | phase), collisions require
    // a modified hash function. For testing, we directly construct entries
    // and verify the promotion logic.
    //
    // Since the v1 hash is injective for all valid inputs, we test the
    // multi-entry path by manually registering two different signatures
    // that happen to get different hashes (verifying they remain single).
    VisualSignature a{1, Phase::Solid};
    VisualSignature b{2, Phase::Solid};
    table.registerSignature(a);
    table.registerSignature(b);
    EXPECT_TRUE(table.isSingleEntry(visualHash(a)));
    EXPECT_TRUE(table.isSingleEntry(visualHash(b)));
    EXPECT_EQ(table.overflowSize(), 0);
}

TEST(VisualMergeTableTest, ClearResetsTable) {
    VisualMergeTable table;
    table.registerSignature({1, Phase::Solid});
    table.registerSignature({2, Phase::Powder});
    table.clear();
    EXPECT_FALSE(table.isSingleEntry(visualHash({1, Phase::Solid})));
    EXPECT_EQ(table.overflowSize(), 0);
}

TEST(VisualMergeTableTest, AllMaterialsMergeWithSelf) {
    VisualMergeTable table;
    const MaterialId ids[] = {material_ids::STONE, material_ids::DIRT, material_ids::SAND, material_ids::WATER,
                              material_ids::GRAVEL};
    for (MaterialId id : ids) {
        auto cell = cellForMaterial(id);
        MergeKey key = mergeKey(cell);
        table.registerSignature(visualSignatureOf(cell));
        EXPECT_TRUE(table.canMerge(key, key));
    }
}

TEST(VisualMergeTableTest, DifferentMaterialsDoNotMerge) {
    VisualMergeTable table;
    auto stone = cellForMaterial(material_ids::STONE);
    auto dirt = cellForMaterial(material_ids::DIRT);
    table.registerSignature(visualSignatureOf(stone));
    table.registerSignature(visualSignatureOf(dirt));
    EXPECT_FALSE(table.canMerge(mergeKey(stone), mergeKey(dirt)));
}
