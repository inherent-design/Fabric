#include "recurse/character/VoxelInteraction.hh"
#include "recurse/simulation/CellAccessors.hh"
#include <gtest/gtest.h>

using namespace fabric;
using namespace recurse::simulation;
using namespace recurse;

class VoxelInteractionTest : public ::testing::Test {
  protected:
    SimulationGrid grid;
    MaterialRegistry registry;

    void SetUp() override {
        // Ensure chunks exist for voxel operations at origin area and neighbors
        grid.fillChunk(0, 0, 0, VoxelCell{});
        grid.fillChunk(-1, -1, -1, VoxelCell{});
        grid.fillChunk(0, 0, -1, VoxelCell{});
        grid.fillChunk(1, 0, 0, VoxelCell{});
        // Advance epoch so reads see fill values
        grid.advanceEpoch();
    }
};

TEST_F(VoxelInteractionTest, CreateMatterPlacesAdjacentPosZ) {
    VoxelInteraction vi(grid, registry);
    VoxelHit hit{5, 5, 5, 0, 0, 1, 1.0f}; // normal +Z
    auto result = vi.createMatter(hit);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.x, 5);
    EXPECT_EQ(result.y, 5);
    EXPECT_EQ(result.z, 6);
    EXPECT_EQ(cellMaterialId(result.newCell), material_ids::SAND);
    EXPECT_EQ(result.source, ChangeSource::Place);
    EXPECT_EQ(cellMaterialId(grid.readCell(5, 5, 6)), material_ids::AIR);
}

TEST_F(VoxelInteractionTest, CreateMatterPlacesAdjacentNegZ) {
    VoxelInteraction vi(grid, registry);
    VoxelHit hit{5, 5, 5, 0, 0, -1, 1.0f};
    auto result = vi.createMatter(hit);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.z, 4);
    EXPECT_EQ(cellMaterialId(result.newCell), material_ids::SAND);
    EXPECT_EQ(cellMaterialId(grid.readCell(5, 5, 4)), material_ids::AIR);
}

TEST_F(VoxelInteractionTest, CreateMatterPlacesAdjacentPosX) {
    VoxelInteraction vi(grid, registry);
    VoxelHit hit{5, 5, 5, 1, 0, 0, 1.0f};
    auto result = vi.createMatter(hit);
    EXPECT_EQ(result.x, 6);
    EXPECT_EQ(cellMaterialId(result.newCell), material_ids::SAND);
    EXPECT_EQ(cellMaterialId(grid.readCell(6, 5, 5)), material_ids::AIR);
}

TEST_F(VoxelInteractionTest, CreateMatterPlacesAdjacentNegX) {
    VoxelInteraction vi(grid, registry);
    VoxelHit hit{5, 5, 5, -1, 0, 0, 1.0f};
    auto result = vi.createMatter(hit);
    EXPECT_EQ(result.x, 4);
}

TEST_F(VoxelInteractionTest, CreateMatterPlacesAdjacentPosY) {
    VoxelInteraction vi(grid, registry);
    VoxelHit hit{5, 5, 5, 0, 1, 0, 1.0f};
    auto result = vi.createMatter(hit);
    EXPECT_EQ(result.y, 6);
}

TEST_F(VoxelInteractionTest, CreateMatterPlacesAdjacentNegY) {
    VoxelInteraction vi(grid, registry);
    VoxelHit hit{5, 5, 5, 0, -1, 0, 1.0f};
    auto result = vi.createMatter(hit);
    EXPECT_EQ(result.y, 4);
}

TEST_F(VoxelInteractionTest, DestroyMatterSetsMaterialToAir) {
    VoxelInteraction vi(grid, registry);
    grid.writeCellImmediate(5, 5, 5, makeCell(static_cast<uint8_t>(material_ids::STONE), Phase::Solid, 200));
    VoxelHit hit{5, 5, 5, 0, 0, -1, 1.0f};
    auto result = vi.destroyMatter(hit);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.x, 5);
    EXPECT_EQ(cellMaterialId(result.newCell), material_ids::AIR);
    EXPECT_EQ(result.source, ChangeSource::Destroy);
    EXPECT_EQ(cellMaterialId(grid.readCell(5, 5, 5)), material_ids::STONE);
}

TEST_F(VoxelInteractionTest, CreateMatterReturnsRequestedCellAndSource) {
    VoxelInteraction vi(grid, registry);
    VoxelHit hit{5, 5, 5, 0, 0, 1, 1.0f};
    auto result = vi.createMatter(hit);
    ASSERT_TRUE(result.success);
    EXPECT_EQ(cellMaterialId(result.newCell), material_ids::SAND);
    EXPECT_NE(result.newCell.flags() & voxel_flags::UPDATED, 0);
    EXPECT_EQ(result.source, ChangeSource::Place);
}

TEST_F(VoxelInteractionTest, DestroyMatterReturnsRequestedCellAndSource) {
    VoxelInteraction vi(grid, registry);
    grid.writeCellImmediate(5, 5, 5, makeCell(static_cast<uint8_t>(material_ids::STONE), Phase::Solid, 200));
    VoxelHit hit{5, 5, 5, 0, 0, -1, 1.0f};
    auto result = vi.destroyMatter(hit);
    ASSERT_TRUE(result.success);
    EXPECT_EQ(cellMaterialId(result.newCell), material_ids::AIR);
    EXPECT_NE(result.newCell.flags() & voxel_flags::UPDATED, 0);
    EXPECT_EQ(result.source, ChangeSource::Destroy);
}

TEST_F(VoxelInteractionTest, CreateMatterAtWithRaycast) {
    VoxelInteraction vi(grid, registry);
    grid.writeCellImmediate(5, 5, 5, makeCell(static_cast<uint8_t>(material_ids::STONE), Phase::Solid, 200));
    auto result = vi.createMatterAt(5.5f, 5.5f, 0.5f, 0.0f, 0.0f, 1.0f);
    EXPECT_TRUE(result.success);
    // Ray hits +Z face of voxel at (5,5,5), normal is (0,0,-1), so placement at (5,5,4)
    EXPECT_EQ(result.z, 4);
    EXPECT_EQ(cellMaterialId(result.newCell), material_ids::SAND);
    EXPECT_EQ(cellMaterialId(grid.readCell(5, 5, 4)), material_ids::AIR);
}

TEST_F(VoxelInteractionTest, DestroyMatterAtWithRaycast) {
    VoxelInteraction vi(grid, registry);
    grid.writeCellImmediate(5, 5, 5, makeCell(static_cast<uint8_t>(material_ids::STONE), Phase::Solid, 200));
    auto result = vi.destroyMatterAt(5.5f, 5.5f, 0.5f, 0.0f, 0.0f, 1.0f);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(cellMaterialId(result.newCell), material_ids::AIR);
    EXPECT_EQ(cellMaterialId(grid.readCell(5, 5, 5)), material_ids::STONE);
}

TEST_F(VoxelInteractionTest, CreateMatterAtNoHitReturnsFail) {
    VoxelInteraction vi(grid, registry);
    auto result = vi.createMatterAt(0.5f, 0.5f, 0.5f, 1.0f, 0.0f, 0.0f);
    EXPECT_FALSE(result.success);
}

TEST_F(VoxelInteractionTest, DestroyMatterAtNoHitReturnsFail) {
    VoxelInteraction vi(grid, registry);
    auto result = vi.destroyMatterAt(0.5f, 0.5f, 0.5f, 1.0f, 0.0f, 0.0f);
    EXPECT_FALSE(result.success);
}

TEST_F(VoxelInteractionTest, WouldOverlapDetectsIntersection) {
    AABB player(Vec3f(4.5f, 4.5f, 4.5f), Vec3f(5.5f, 6.5f, 5.5f));
    EXPECT_TRUE(VoxelInteraction::wouldOverlap(5, 5, 5, player));
}

TEST_F(VoxelInteractionTest, WouldOverlapNoIntersection) {
    AABB player(Vec3f(0.0f, 0.0f, 0.0f), Vec3f(1.0f, 2.0f, 1.0f));
    EXPECT_FALSE(VoxelInteraction::wouldOverlap(5, 5, 5, player));
}

TEST_F(VoxelInteractionTest, NegativeCoordinatesWork) {
    VoxelInteraction vi(grid, registry);
    VoxelHit hit{-5, -3, -7, 0, 0, -1, 1.0f};
    auto result = vi.createMatter(hit);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.x, -5);
    EXPECT_EQ(result.y, -3);
    EXPECT_EQ(result.z, -8);
    EXPECT_EQ(cellMaterialId(result.newCell), material_ids::SAND);
    EXPECT_EQ(cellMaterialId(grid.readCell(-5, -3, -8)), material_ids::AIR);
}

TEST_F(VoxelInteractionTest, ChunkCoordsCalculatedCorrectly) {
    VoxelInteraction vi(grid, registry);
    VoxelHit hit{31, 0, 0, 1, 0, 0, 1.0f}; // normal +X, target = (32, 0, 0)
    auto result = vi.createMatter(hit);
    EXPECT_EQ(result.x, 32);
    EXPECT_EQ(result.cx, 1); // 32 >> 5 = 1
    EXPECT_EQ(result.cy, 0);
    EXPECT_EQ(result.cz, 0);
}
