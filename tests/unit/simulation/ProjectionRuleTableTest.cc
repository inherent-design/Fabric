#include "recurse/simulation/ProjectionRuleTable.hh"
#include "recurse/simulation/MaterialRegistry.hh"

#include <gtest/gtest.h>

using namespace recurse::simulation;

// -- Default construction -----------------------------------------------------

TEST(ProjectionRuleTableTest, DefaultLookupReturnsEmptyMaterial) {
    ProjectionRuleTable table;
    const auto& mat = table.lookup(0, Phase::Empty);
    EXPECT_EQ(mat.baseColor, 0u);
    EXPECT_EQ(mat.soundCategory, 0);
    EXPECT_EQ(mat.semanticPriority, 0);
    EXPECT_EQ(mat.moveType, MoveType::Static);
    EXPECT_EQ(mat.density, 0);
    EXPECT_TRUE(mat.displayName.empty());
}

TEST(ProjectionRuleTableTest, DefaultLookupNonZeroIndexReturnsEmpty) {
    ProjectionRuleTable table;
    const auto& mat = table.lookup(42, Phase::Solid);
    EXPECT_EQ(mat.baseColor, 0u);
    EXPECT_EQ(mat.density, 0);
}

// -- setRule + lookup round-trip ----------------------------------------------

TEST(ProjectionRuleTableTest, SetRuleLookupRoundTrip) {
    ProjectionRuleTable table;

    ProjectedMaterial rule;
    rule.displayName = "TestMaterial";
    rule.baseColor = 0xFFAA5500;
    rule.soundCategory = 3;
    rule.semanticPriority = 10;
    rule.moveType = MoveType::Powder;
    rule.density = 130;

    table.setRule(5, Phase::Powder, rule);

    const auto& result = table.lookup(5, Phase::Powder);
    EXPECT_EQ(result.displayName, "TestMaterial");
    EXPECT_EQ(result.baseColor, 0xFFAA5500u);
    EXPECT_EQ(result.soundCategory, 3);
    EXPECT_EQ(result.semanticPriority, 10);
    EXPECT_EQ(result.moveType, MoveType::Powder);
    EXPECT_EQ(result.density, 130);
}

// -- Multiple phases for same essenceIdx --------------------------------------

TEST(ProjectionRuleTableTest, IndependentPhaseEntries) {
    ProjectionRuleTable table;

    ProjectedMaterial solid;
    solid.baseColor = 0xFF000001;
    solid.moveType = MoveType::Static;
    table.setRule(1, Phase::Solid, solid);

    ProjectedMaterial liquid;
    liquid.baseColor = 0xFF000002;
    liquid.moveType = MoveType::Liquid;
    table.setRule(1, Phase::Liquid, liquid);

    EXPECT_EQ(table.lookup(1, Phase::Solid).baseColor, 0xFF000001u);
    EXPECT_EQ(table.lookup(1, Phase::Liquid).baseColor, 0xFF000002u);
    // Unset phases for the same essenceIdx remain default
    EXPECT_EQ(table.lookup(1, Phase::Empty).baseColor, 0u);
    EXPECT_EQ(table.lookup(1, Phase::Powder).baseColor, 0u);
    EXPECT_EQ(table.lookup(1, Phase::Gas).baseColor, 0u);
}

// -- Boundary values ----------------------------------------------------------

TEST(ProjectionRuleTableTest, EssenceIdxZeroAir) {
    ProjectionRuleTable table;

    ProjectedMaterial air;
    air.baseColor = 0x00000000;
    air.moveType = MoveType::Static;
    table.setRule(0, Phase::Empty, air);

    const auto& result = table.lookup(0, Phase::Empty);
    EXPECT_EQ(result.baseColor, 0u);
}

TEST(ProjectionRuleTableTest, EssenceIdxMax) {
    ProjectionRuleTable table;

    ProjectedMaterial rare;
    rare.baseColor = 0xFFFF00FF;
    rare.density = 255;
    table.setRule(255, Phase::Gas, rare);

    const auto& result = table.lookup(255, Phase::Gas);
    EXPECT_EQ(result.baseColor, 0xFFFF00FFu);
    EXPECT_EQ(result.density, 255);
}

// -- Phase enum coverage ------------------------------------------------------

TEST(ProjectionRuleTableTest, AllPhasesProduceValidIndices) {
    ProjectionRuleTable table;
    const Phase phases[] = {Phase::Empty, Phase::Solid, Phase::Powder, Phase::Liquid, Phase::Gas};

    for (auto p : phases) {
        ProjectedMaterial mat;
        mat.baseColor = static_cast<uint32_t>(p) + 1;
        table.setRule(10, p, mat);
    }

    for (auto p : phases) {
        EXPECT_EQ(table.lookup(10, p).baseColor, static_cast<uint32_t>(p) + 1);
    }
}

// -- populateFromRegistry -----------------------------------------------------

TEST(ProjectionRuleTableTest, PopulateFromRegistryStone) {
    MaterialRegistry registry;
    ProjectionRuleTable table;
    table.populateFromRegistry(registry);

    // Stone: MaterialId=1, MoveType::Static -> Phase::Solid
    const auto& stone = table.lookup(static_cast<uint8_t>(material_ids::STONE), Phase::Solid);
    EXPECT_EQ(stone.baseColor, 0xFF808080u);
    EXPECT_EQ(stone.moveType, MoveType::Static);
    EXPECT_EQ(stone.density, 200);
}

TEST(ProjectionRuleTableTest, PopulateFromRegistrySand) {
    MaterialRegistry registry;
    ProjectionRuleTable table;
    table.populateFromRegistry(registry);

    // Sand: MaterialId=3, MoveType::Powder -> Phase::Powder
    const auto& sand = table.lookup(static_cast<uint8_t>(material_ids::SAND), Phase::Powder);
    EXPECT_EQ(sand.baseColor, 0xFFC2B280u);
    EXPECT_EQ(sand.moveType, MoveType::Powder);
    EXPECT_EQ(sand.density, 130);
}

TEST(ProjectionRuleTableTest, PopulateFromRegistryWater) {
    MaterialRegistry registry;
    ProjectionRuleTable table;
    table.populateFromRegistry(registry);

    // Water: MaterialId=4, MoveType::Liquid -> Phase::Liquid
    const auto& water = table.lookup(static_cast<uint8_t>(material_ids::WATER), Phase::Liquid);
    EXPECT_EQ(water.baseColor, 0xFF4040C0u);
    EXPECT_EQ(water.moveType, MoveType::Liquid);
    EXPECT_EQ(water.density, 100);
}

TEST(ProjectionRuleTableTest, PopulateFromRegistryAir) {
    MaterialRegistry registry;
    ProjectionRuleTable table;
    table.populateFromRegistry(registry);

    // Air: MaterialId=0, AIR -> Phase::Empty
    const auto& air = table.lookup(0, Phase::Empty);
    EXPECT_EQ(air.baseColor, 0x00000000u);
    EXPECT_EQ(air.moveType, MoveType::Static);
    EXPECT_EQ(air.density, 0);
}

// -- Overwrite after populate -------------------------------------------------

TEST(ProjectionRuleTableTest, SetRuleOverridesPopulated) {
    MaterialRegistry registry;
    ProjectionRuleTable table;
    table.populateFromRegistry(registry);

    ProjectedMaterial custom;
    custom.baseColor = 0xFFDEAD00;
    custom.density = 42;
    table.setRule(static_cast<uint8_t>(material_ids::STONE), Phase::Solid, custom);

    const auto& result = table.lookup(static_cast<uint8_t>(material_ids::STONE), Phase::Solid);
    EXPECT_EQ(result.baseColor, 0xFFDEAD00u);
    EXPECT_EQ(result.density, 42);
}
