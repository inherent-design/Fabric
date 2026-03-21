#include "recurse/simulation/CellAccessors.hh"
#include "recurse/simulation/MaterialRegistry.hh"
#include "recurse/simulation/MatterState.hh"
#include "recurse/simulation/ProjectionRuleTable.hh"
#include "recurse/simulation/VoxelMaterial.hh"

#include <gtest/gtest.h>

using namespace recurse::simulation;

// -- phaseFromMoveType -------------------------------------------------------

TEST(CellFactoryTest, PhaseFromMoveTypeStatic) {
    EXPECT_EQ(phaseFromMoveType(MoveType::Static), Phase::Solid);
}

TEST(CellFactoryTest, PhaseFromMoveTypePowder) {
    EXPECT_EQ(phaseFromMoveType(MoveType::Powder), Phase::Powder);
}

TEST(CellFactoryTest, PhaseFromMoveTypeLiquid) {
    EXPECT_EQ(phaseFromMoveType(MoveType::Liquid), Phase::Liquid);
}

TEST(CellFactoryTest, PhaseFromMoveTypeGas) {
    EXPECT_EQ(phaseFromMoveType(MoveType::Gas), Phase::Gas);
}

// -- makeCell ----------------------------------------------------------------

TEST(CellFactoryTest, MakeCellSetsEssenceIdx) {
    auto cell = makeCell(42, Phase::Solid);
    EXPECT_EQ(cell.essenceIdx, 42);
}

TEST(CellFactoryTest, MakeCellSetsPhase) {
    auto cell = makeCell(1, Phase::Liquid);
    EXPECT_EQ(cell.phase(), Phase::Liquid);
}

TEST(CellFactoryTest, MakeCellSetsDisplacementRank) {
    auto cell = makeCell(1, Phase::Powder, 130);
    EXPECT_EQ(cell.displacementRank, 130);
}

TEST(CellFactoryTest, MakeCellSetsFlags) {
    auto cell = makeCell(1, Phase::Solid, 0, 0x1F);
    EXPECT_EQ(cell.flags(), 0x1F);
}

TEST(CellFactoryTest, MakeCellDefaultDisplacementRankIsZero) {
    auto cell = makeCell(1, Phase::Solid);
    EXPECT_EQ(cell.displacementRank, 0);
}

TEST(CellFactoryTest, MakeCellDefaultFlagsIsZero) {
    auto cell = makeCell(1, Phase::Solid);
    EXPECT_EQ(cell.flags(), 0);
}

TEST(CellFactoryTest, MakeCellSpareIsZero) {
    auto cell = makeCell(5, Phase::Gas, 100, 0x0A);
    EXPECT_EQ(cell.spare, 0);
}

// -- emptyCell ---------------------------------------------------------------

TEST(CellFactoryTest, EmptyCellIsAllZero) {
    auto cell = emptyCell();
    EXPECT_EQ(cell.essenceIdx, 0);
    EXPECT_EQ(cell.displacementRank, 0);
    EXPECT_EQ(cell.phaseAndFlags, 0);
    EXPECT_EQ(cell.spare, 0);
}

TEST(CellFactoryTest, EmptyCellPhaseIsEmpty) {
    auto cell = emptyCell();
    EXPECT_EQ(cell.phase(), Phase::Empty);
}

TEST(CellFactoryTest, EmptyCellIsEmptyViaAccessor) {
    auto cell = emptyCell();
    EXPECT_TRUE(isEmpty(cell));
    EXPECT_FALSE(isOccupied(cell));
}

// -- makeCellFromMaterial ----------------------------------------------------

class CellFactoryMaterialTest : public ::testing::Test {
  protected:
    MaterialRegistry registry;
};

TEST_F(CellFactoryMaterialTest, AirIsPhaseEmpty) {
    auto cell = makeCellFromMaterial(material_ids::AIR, registry);
    EXPECT_EQ(cell.phase(), Phase::Empty);
    EXPECT_EQ(cell.essenceIdx, 0);
    EXPECT_EQ(cell.displacementRank, registry.get(material_ids::AIR).density);
}

TEST_F(CellFactoryMaterialTest, StoneIsPhaseSolid) {
    auto cell = makeCellFromMaterial(material_ids::STONE, registry);
    EXPECT_EQ(cell.phase(), Phase::Solid);
    EXPECT_EQ(cell.essenceIdx, static_cast<uint8_t>(material_ids::STONE));
    EXPECT_EQ(cell.displacementRank, registry.get(material_ids::STONE).density);
}

TEST_F(CellFactoryMaterialTest, WaterIsPhaseLiquid) {
    auto cell = makeCellFromMaterial(material_ids::WATER, registry);
    EXPECT_EQ(cell.phase(), Phase::Liquid);
    EXPECT_EQ(cell.essenceIdx, static_cast<uint8_t>(material_ids::WATER));
    EXPECT_EQ(cell.displacementRank, registry.get(material_ids::WATER).density);
}

TEST_F(CellFactoryMaterialTest, SandIsPhasePowder) {
    auto cell = makeCellFromMaterial(material_ids::SAND, registry);
    EXPECT_EQ(cell.phase(), Phase::Powder);
    EXPECT_EQ(cell.essenceIdx, static_cast<uint8_t>(material_ids::SAND));
    EXPECT_EQ(cell.displacementRank, registry.get(material_ids::SAND).density);
}

TEST_F(CellFactoryMaterialTest, GravelIsPhasePowder) {
    auto cell = makeCellFromMaterial(material_ids::GRAVEL, registry);
    EXPECT_EQ(cell.phase(), Phase::Powder);
    EXPECT_EQ(cell.essenceIdx, static_cast<uint8_t>(material_ids::GRAVEL));
    EXPECT_EQ(cell.displacementRank, registry.get(material_ids::GRAVEL).density);
}

TEST_F(CellFactoryMaterialTest, DirtIsPhaseSolid) {
    auto cell = makeCellFromMaterial(material_ids::DIRT, registry);
    EXPECT_EQ(cell.phase(), Phase::Solid);
    EXPECT_EQ(cell.essenceIdx, static_cast<uint8_t>(material_ids::DIRT));
    EXPECT_EQ(cell.displacementRank, registry.get(material_ids::DIRT).density);
}

TEST_F(CellFactoryMaterialTest, FlagsPassedThrough) {
    auto cell = makeCellFromMaterial(material_ids::STONE, registry, 0x0B);
    EXPECT_EQ(cell.flags(), 0x0B);
    EXPECT_EQ(cell.phase(), Phase::Solid);
}

// -- Round-trip tests --------------------------------------------------------

TEST(CellFactoryTest, MakeCellPhaseRoundTrip) {
    const Phase phases[] = {Phase::Empty, Phase::Solid, Phase::Powder, Phase::Liquid, Phase::Gas};
    for (auto p : phases) {
        auto cell = makeCell(0, p);
        EXPECT_EQ(cell.phase(), p);
    }
}

TEST(CellFactoryTest, MakeCellFlagsRoundTrip) {
    for (uint8_t f = 0; f <= 0x1F; ++f) {
        auto cell = makeCell(0, Phase::Solid, 0, f);
        EXPECT_EQ(cell.flags(), f);
    }
}

TEST(CellFactoryTest, MakeCellPhaseAndFlagsIndependent) {
    auto cell = makeCell(7, Phase::Gas, 200, 0x15);
    EXPECT_EQ(cell.essenceIdx, 7);
    EXPECT_EQ(cell.phase(), Phase::Gas);
    EXPECT_EQ(cell.displacementRank, 200);
    EXPECT_EQ(cell.flags(), 0x15);
    EXPECT_EQ(cell.spare, 0);
}

// -- Projection-aware accessors (MatterState) --------------------------------

TEST(ProjectionAccessorTest, CellMaterialIdEmptyReturnsAir) {
    auto cell = emptyCell();
    EXPECT_EQ(cellMaterialId(cell), material_ids::AIR);
}

TEST(ProjectionAccessorTest, CellMaterialIdSandReturns3) {
    auto cell = makeCell(static_cast<uint8_t>(material_ids::SAND), Phase::Powder);
    EXPECT_EQ(cellMaterialId(cell), material_ids::SAND);
}

TEST(ProjectionAccessorTest, CellMaterialIdStoneReturns1) {
    auto cell = makeCell(static_cast<uint8_t>(material_ids::STONE), Phase::Solid);
    EXPECT_EQ(cellMaterialId(cell), material_ids::STONE);
}

TEST(ProjectionAccessorTest, MergeKeyEmptyReturnsEmpty) {
    auto cell = emptyCell();
    EXPECT_EQ(mergeKey(cell), K_MERGE_KEY_EMPTY);
}

TEST(ProjectionAccessorTest, MergeKeyReturnsEssenceIdx) {
    auto cell = makeCell(static_cast<uint8_t>(material_ids::WATER), Phase::Liquid);
    EXPECT_EQ(mergeKey(cell), static_cast<MergeKey>(material_ids::WATER));
}

TEST(ProjectionAccessorTest, SemanticPrioritySandReturns4) {
    MatterState cell;
    cell.essenceIdx = static_cast<uint8_t>(material_ids::SAND);
    cell.setPhase(Phase::Powder);
    EXPECT_EQ(materialSemanticPriority(cell), 4);
}

TEST(ProjectionAccessorTest, SemanticPriorityStoneReturns1) {
    MatterState cell;
    cell.essenceIdx = static_cast<uint8_t>(material_ids::STONE);
    cell.setPhase(Phase::Solid);
    EXPECT_EQ(materialSemanticPriority(cell), 1);
}

TEST(ProjectionAccessorTest, SemanticPriorityAirReturns0) {
    MatterState cell{};
    EXPECT_EQ(materialSemanticPriority(cell), 0);
}

TEST(ProjectionAccessorTest, TableCellMaterialIdMatchesNonTable) {
    ProjectionRuleTable table;
    MatterState cell;
    cell.essenceIdx = static_cast<uint8_t>(material_ids::SAND);
    cell.setPhase(Phase::Powder);
    EXPECT_EQ(cellMaterialId(table, cell), cellMaterialId(cell));
}

TEST(ProjectionAccessorTest, TableCellMaterialIdEmptyMatchesNonTable) {
    ProjectionRuleTable table;
    MatterState cell{};
    EXPECT_EQ(cellMaterialId(table, cell), cellMaterialId(cell));
}
