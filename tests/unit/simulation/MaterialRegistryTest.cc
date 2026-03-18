#include "recurse/simulation/MaterialRegistry.hh"
#include "recurse/simulation/EssenceColor.hh"
#include "recurse/simulation/VoxelMaterial.hh"

#include <cmath>
#include <gtest/gtest.h>

using namespace recurse::simulation;

TEST(VoxelCellTest, SizeIs4Bytes) {
    EXPECT_EQ(sizeof(VoxelCell), 4u);
}

TEST(VoxelCellTest, DefaultIsAir) {
    VoxelCell cell;
    EXPECT_EQ(cell.materialId, material_ids::AIR);
    EXPECT_EQ(cell.flags, voxel_flags::NONE);
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
