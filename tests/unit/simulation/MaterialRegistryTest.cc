#include "fabric/simulation/MaterialRegistry.hh"
#include "fabric/simulation/VoxelMaterial.hh"
#include <gtest/gtest.h>

using namespace fabric::simulation;

TEST(VoxelCellTest, SizeIs4Bytes) {
    EXPECT_EQ(sizeof(VoxelCell), 4u);
}

TEST(VoxelCellTest, DefaultIsAir) {
    VoxelCell cell;
    EXPECT_EQ(cell.materialId, MaterialIds::Air);
    EXPECT_EQ(cell.flags, VoxelFlags::None);
}

class MaterialRegistryTest : public ::testing::Test {
  protected:
    MaterialRegistry registry;
};

TEST_F(MaterialRegistryTest, CountIs6) {
    EXPECT_EQ(registry.count(), MaterialIds::Count);
}

TEST_F(MaterialRegistryTest, AirIsStaticDensityZero) {
    const auto& air = registry.get(MaterialIds::Air);
    EXPECT_EQ(air.moveType, MoveType::Static);
    EXPECT_EQ(air.density, 0);
    EXPECT_EQ(air.baseColor, 0x00000000u);
}

TEST_F(MaterialRegistryTest, StoneIsStatic) {
    const auto& stone = registry.get(MaterialIds::Stone);
    EXPECT_EQ(stone.moveType, MoveType::Static);
    EXPECT_EQ(stone.density, 200);
}

TEST_F(MaterialRegistryTest, DirtIsStatic) {
    const auto& dirt = registry.get(MaterialIds::Dirt);
    EXPECT_EQ(dirt.moveType, MoveType::Static);
    EXPECT_EQ(dirt.density, 150);
}

TEST_F(MaterialRegistryTest, SandIsPowder) {
    const auto& sand = registry.get(MaterialIds::Sand);
    EXPECT_EQ(sand.moveType, MoveType::Powder);
    EXPECT_EQ(sand.density, 130);
}

TEST_F(MaterialRegistryTest, WaterIsLiquid) {
    const auto& water = registry.get(MaterialIds::Water);
    EXPECT_EQ(water.moveType, MoveType::Liquid);
    EXPECT_EQ(water.density, 100);
    EXPECT_EQ(water.viscosity, 10);
    EXPECT_EQ(water.dispersionRate, 3);
}

TEST_F(MaterialRegistryTest, GravelIsPowder) {
    const auto& gravel = registry.get(MaterialIds::Gravel);
    EXPECT_EQ(gravel.moveType, MoveType::Powder);
    EXPECT_EQ(gravel.density, 170);
}

TEST_F(MaterialRegistryTest, DensityOrdering) {
    // Stone(200) > Gravel(170) > Dirt(150) > Sand(130) > Water(100) > Air(0)
    EXPECT_GT(registry.get(MaterialIds::Stone).density, registry.get(MaterialIds::Gravel).density);
    EXPECT_GT(registry.get(MaterialIds::Gravel).density, registry.get(MaterialIds::Dirt).density);
    EXPECT_GT(registry.get(MaterialIds::Dirt).density, registry.get(MaterialIds::Sand).density);
    EXPECT_GT(registry.get(MaterialIds::Sand).density, registry.get(MaterialIds::Water).density);
    EXPECT_GT(registry.get(MaterialIds::Water).density, registry.get(MaterialIds::Air).density);
}
