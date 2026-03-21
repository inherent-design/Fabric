#include "recurse/simulation/MaterialRegistry.hh"
#include "recurse/simulation/CellAccessors.hh"
#include "recurse/simulation/EssenceColor.hh"
#include "recurse/simulation/VoxelMaterial.hh"
#include "recurse/simulation/VoxelSemanticView.hh"
#include "recurse/world/EssencePalette.hh"

#include <cmath>
#include <gtest/gtest.h>

using namespace recurse::simulation;

TEST(VoxelCellTest, SizeIs4Bytes) {
    EXPECT_EQ(sizeof(VoxelCell), 4u);
}

TEST(VoxelCellTest, DefaultIsAir) {
    VoxelCell cell;
    EXPECT_EQ(cell.phase(), Phase::Empty);
    EXPECT_EQ(cell.flags(), voxel_flags::NONE);
}

class MaterialRegistryTest : public ::testing::Test {
  protected:
    MaterialRegistry registry;
};

TEST_F(MaterialRegistryTest, CountIs6) {
    EXPECT_EQ(registry.count(), material_ids::COUNT);
}

TEST_F(MaterialRegistryTest, AirIsStaticDensityZero) {
    const auto& air = registry.get(material_ids::AIR);
    EXPECT_EQ(air.moveType, MoveType::Static);
    EXPECT_EQ(air.density, 0);
    EXPECT_EQ(air.baseColor, 0x00000000u);
}

TEST_F(MaterialRegistryTest, StoneIsStatic) {
    const auto& stone = registry.get(material_ids::STONE);
    EXPECT_EQ(stone.moveType, MoveType::Static);
    EXPECT_EQ(stone.density, 200);
}

TEST_F(MaterialRegistryTest, DirtIsStatic) {
    const auto& dirt = registry.get(material_ids::DIRT);
    EXPECT_EQ(dirt.moveType, MoveType::Static);
    EXPECT_EQ(dirt.density, 150);
}

TEST_F(MaterialRegistryTest, SandIsPowder) {
    const auto& sand = registry.get(material_ids::SAND);
    EXPECT_EQ(sand.moveType, MoveType::Powder);
    EXPECT_EQ(sand.density, 130);
}

TEST_F(MaterialRegistryTest, WaterIsLiquid) {
    const auto& water = registry.get(material_ids::WATER);
    EXPECT_EQ(water.moveType, MoveType::Liquid);
    EXPECT_EQ(water.density, 100);
    EXPECT_EQ(water.viscosity, 10);
    EXPECT_EQ(water.dispersionRate, 3);
}

TEST_F(MaterialRegistryTest, GravelIsPowder) {
    const auto& gravel = registry.get(material_ids::GRAVEL);
    EXPECT_EQ(gravel.moveType, MoveType::Powder);
    EXPECT_EQ(gravel.density, 170);
}

TEST_F(MaterialRegistryTest, DensityOrdering) {
    // Stone(200) > Gravel(170) > Dirt(150) > Sand(130) > Water(100) > Air(0)
    EXPECT_GT(registry.get(material_ids::STONE).density, registry.get(material_ids::GRAVEL).density);
    EXPECT_GT(registry.get(material_ids::GRAVEL).density, registry.get(material_ids::DIRT).density);
    EXPECT_GT(registry.get(material_ids::DIRT).density, registry.get(material_ids::SAND).density);
    EXPECT_GT(registry.get(material_ids::SAND).density, registry.get(material_ids::WATER).density);
    EXPECT_GT(registry.get(material_ids::WATER).density, registry.get(material_ids::AIR).density);
}

TEST_F(MaterialRegistryTest, TerrainAppearanceColorUsesBaseColorContract) {
    const auto& sand = registry.get(material_ids::SAND);
    const auto terrainColor = registry.terrainAppearanceColor(material_ids::SAND);

    EXPECT_FLOAT_EQ(terrainColor[0], 194.0f / 255.0f);
    EXPECT_FLOAT_EQ(terrainColor[1], 178.0f / 255.0f);
    EXPECT_FLOAT_EQ(terrainColor[2], 128.0f / 255.0f);
    EXPECT_FLOAT_EQ(terrainColor[3], 1.0f);

    const fabric::Vector4<float, fabric::Space::World> baseEssence(sand.baseEssence[0], sand.baseEssence[1],
                                                                   sand.baseEssence[2], sand.baseEssence[3]);
    const auto essenceColor = essenceToColor(baseEssence);

    EXPECT_GT(std::fabs(terrainColor[0] - essenceColor[0]), 0.2f);
    EXPECT_GT(std::fabs(terrainColor[1] - essenceColor[1]), 0.1f);
}

TEST_F(MaterialRegistryTest, EssenceValueBridgesCurrentRepresentations) {
    const auto& water = registry.get(material_ids::WATER);
    const auto value = EssenceValue::fromMaterialDef(water);

    const auto asArray = value.toArray();
    const auto asVector = value.toVector();
    const auto asSemantic = value.toSemanticEssence();

    EXPECT_FLOAT_EQ(asArray[0], 0.0f);
    EXPECT_FLOAT_EQ(asArray[2], 0.9f);
    EXPECT_FLOAT_EQ(asVector.z, 0.9f);
    EXPECT_FLOAT_EQ(asSemantic.life, 0.9f);

    const auto roundTrip = EssenceValue::fromSemantic(asSemantic);
    EXPECT_FLOAT_EQ(roundTrip.life, 0.9f);
    EXPECT_EQ(roundTrip.dominant(), recurse::EssenceType::Life);
}

TEST_F(MaterialRegistryTest, MaterialSemanticRegistryMirrorsCurrentMaterialTruth) {
    MaterialSemanticRegistry semantics(registry);

    const auto sand = semantics.view(material_ids::SAND);

    EXPECT_STREQ(sand.displayName, "Sand");
    EXPECT_EQ(sand.moveType, MoveType::Powder);
    EXPECT_STREQ(sand.moveTypeName, "Powder");
    EXPECT_EQ(sand.materialDensity, 130);
    EXPECT_EQ(sand.viscosity, 0);
    EXPECT_TRUE(sand.occupancy.occupied);
    EXPECT_TRUE(sand.occupancy.blocksRaycast);
    EXPECT_FLOAT_EQ(sand.occupancy.density, 1.0f);
    EXPECT_FLOAT_EQ(sand.terrainAppearance.color[0], 194.0f / 255.0f);
    EXPECT_FLOAT_EQ(sand.intrinsicEssence.order, 0.3f);
    EXPECT_FLOAT_EQ(sand.intrinsicEssence.life, 0.3f);
}

TEST_F(MaterialRegistryTest, ResolveVoxelSemanticsUsesChunkLocalPaletteAsOptionalContext) {
    MaterialSemanticRegistry semantics(registry);
    recurse::EssencePalette palette;

    // In the new layout essenceIdx IS the material identity, so the palette
    // entry must sit at the same index as the material id.  STONE == 1, so
    // we need a dummy at index 0 and the real entry at index 1.
    palette.addEntryRaw({0.0f, 0.0f, 0.0f, 0.0f});                       // index 0 (dummy)
    const auto stoneIdx = palette.addEntryRaw({0.1f, 0.2f, 0.3f, 0.4f}); // index 1
    ASSERT_EQ(stoneIdx, static_cast<uint8_t>(material_ids::STONE));

    VoxelCell cell = makeCell(static_cast<uint8_t>(material_ids::STONE), Phase::Solid, 200);

    const auto resolved = semantics.resolve(cell, palette);

    EXPECT_STREQ(resolved.material.displayName, "Stone");
    EXPECT_EQ(resolved.sampledEssence.index, stoneIdx);
    EXPECT_TRUE(resolved.sampledEssence.hasPalette);
    EXPECT_TRUE(resolved.sampledEssence.inRange);
    ASSERT_TRUE(resolved.sampledEssence.value.has_value());
    EXPECT_FLOAT_EQ(resolved.sampledEssence.value->order, 0.1f);
    EXPECT_FLOAT_EQ(resolved.sampledEssence.value->chaos, 0.2f);
    EXPECT_FLOAT_EQ(resolved.sampledEssence.value->life, 0.3f);
    EXPECT_FLOAT_EQ(resolved.sampledEssence.value->decay, 0.4f);
}

TEST_F(MaterialRegistryTest, ResolveVoxelSemanticsDoesNotTreatEssenceIndexAsCanonicalWithoutPalette) {
    MaterialSemanticRegistry semantics(registry);

    // essenceIdx == material identity in the new layout, so just use DIRT
    // directly.  Without a palette, resolve should fall back to intrinsic
    // essence from the MaterialDef.
    VoxelCell cell = makeCell(static_cast<uint8_t>(material_ids::DIRT), Phase::Solid, 150);

    const auto resolved = semantics.resolve(cell, nullptr);

    EXPECT_STREQ(resolved.material.displayName, "Dirt");
    EXPECT_FALSE(resolved.sampledEssence.hasPalette);
    EXPECT_FALSE(resolved.sampledEssence.inRange);
    EXPECT_FALSE(resolved.sampledEssence.value.has_value());
    EXPECT_FLOAT_EQ(resolved.material.intrinsicEssence.order, 0.2f);
    EXPECT_FLOAT_EQ(resolved.material.intrinsicEssence.life, 0.6f);
}
