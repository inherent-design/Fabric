#include "recurse/gameplay/VoxelInteraction.hh"
#include <gtest/gtest.h>

using namespace fabric;
using namespace fabric::simulation;
using namespace recurse;

class VoxelInteractionTest : public ::testing::Test {
  protected:
    SimulationGrid grid;
    EventDispatcher dispatcher;

    void SetUp() override {
        eventCount = 0;
        dispatcher.addEventListener(K_VOXEL_CHANGED_EVENT, [this](Event&) { ++eventCount; });
        // Ensure chunks exist for voxel operations at origin area and neighbors
        grid.fillChunk(0, 0, 0, VoxelCell{});
        grid.fillChunk(-1, -1, -1, VoxelCell{});
        grid.fillChunk(0, 0, -1, VoxelCell{});
        grid.fillChunk(1, 0, 0, VoxelCell{});
        // Advance epoch so reads see fill values
        grid.advanceEpoch();
    }

    int eventCount = 0;
};

TEST_F(VoxelInteractionTest, CreateMatterPlacesAdjacentPosZ) {
    VoxelInteraction vi(grid, dispatcher);
    VoxelHit hit{5, 5, 5, 0, 0, 1, 1.0f}; // normal +Z
    auto result = vi.createMatter(hit);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.x, 5);
    EXPECT_EQ(result.y, 5);
    EXPECT_EQ(result.z, 6);
    EXPECT_NE(grid.readCell(5, 5, 6).materialId, material_ids::AIR);
}

TEST_F(VoxelInteractionTest, CreateMatterPlacesAdjacentNegZ) {
    VoxelInteraction vi(grid, dispatcher);
    VoxelHit hit{5, 5, 5, 0, 0, -1, 1.0f};
    auto result = vi.createMatter(hit);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.z, 4);
    EXPECT_NE(grid.readCell(5, 5, 4).materialId, material_ids::AIR);
}

TEST_F(VoxelInteractionTest, CreateMatterPlacesAdjacentPosX) {
    VoxelInteraction vi(grid, dispatcher);
    VoxelHit hit{5, 5, 5, 1, 0, 0, 1.0f};
    auto result = vi.createMatter(hit);
    EXPECT_EQ(result.x, 6);
    EXPECT_NE(grid.readCell(6, 5, 5).materialId, material_ids::AIR);
}

TEST_F(VoxelInteractionTest, CreateMatterPlacesAdjacentNegX) {
    VoxelInteraction vi(grid, dispatcher);
    VoxelHit hit{5, 5, 5, -1, 0, 0, 1.0f};
    auto result = vi.createMatter(hit);
    EXPECT_EQ(result.x, 4);
}

TEST_F(VoxelInteractionTest, CreateMatterPlacesAdjacentPosY) {
    VoxelInteraction vi(grid, dispatcher);
    VoxelHit hit{5, 5, 5, 0, 1, 0, 1.0f};
    auto result = vi.createMatter(hit);
    EXPECT_EQ(result.y, 6);
}

TEST_F(VoxelInteractionTest, CreateMatterPlacesAdjacentNegY) {
    VoxelInteraction vi(grid, dispatcher);
    VoxelHit hit{5, 5, 5, 0, -1, 0, 1.0f};
    auto result = vi.createMatter(hit);
    EXPECT_EQ(result.y, 4);
}

TEST_F(VoxelInteractionTest, DestroyMatterSetsMaterialToAir) {
    VoxelInteraction vi(grid, dispatcher);
    grid.writeCellImmediate(5, 5, 5, VoxelCell{material_ids::STONE, 128, voxel_flags::NONE});
    VoxelHit hit{5, 5, 5, 0, 0, -1, 1.0f};
    auto result = vi.destroyMatter(hit);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.x, 5);
    EXPECT_EQ(grid.readCell(5, 5, 5).materialId, material_ids::AIR);
}

TEST_F(VoxelInteractionTest, CreateMatterEmitsVoxelChangedEvent) {
    VoxelInteraction vi(grid, dispatcher);
    VoxelHit hit{5, 5, 5, 0, 0, 1, 1.0f};
    vi.createMatter(hit);
    EXPECT_EQ(eventCount, 1);
}

TEST_F(VoxelInteractionTest, DestroyMatterEmitsVoxelChangedEvent) {
    VoxelInteraction vi(grid, dispatcher);
    grid.writeCellImmediate(5, 5, 5, VoxelCell{material_ids::STONE, 128, voxel_flags::NONE});
    VoxelHit hit{5, 5, 5, 0, 0, -1, 1.0f};
    vi.destroyMatter(hit);
    EXPECT_EQ(eventCount, 1);
}

TEST_F(VoxelInteractionTest, CreateMatterAtWithRaycast) {
    VoxelInteraction vi(grid, dispatcher);
    grid.writeCellImmediate(5, 5, 5, VoxelCell{material_ids::STONE, 128, voxel_flags::NONE});
    auto result = vi.createMatterAt(5.5f, 5.5f, 0.5f, 0.0f, 0.0f, 1.0f);
    EXPECT_TRUE(result.success);
    // Ray hits +Z face of voxel at (5,5,5), normal is (0,0,-1), so placement at (5,5,4)
    EXPECT_EQ(result.z, 4);
    EXPECT_NE(grid.readCell(5, 5, 4).materialId, material_ids::AIR);
}

TEST_F(VoxelInteractionTest, DestroyMatterAtWithRaycast) {
    VoxelInteraction vi(grid, dispatcher);
    grid.writeCellImmediate(5, 5, 5, VoxelCell{material_ids::STONE, 128, voxel_flags::NONE});
    auto result = vi.destroyMatterAt(5.5f, 5.5f, 0.5f, 0.0f, 0.0f, 1.0f);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(grid.readCell(5, 5, 5).materialId, material_ids::AIR);
}

TEST_F(VoxelInteractionTest, CreateMatterAtNoHitReturnsFail) {
    VoxelInteraction vi(grid, dispatcher);
    auto result = vi.createMatterAt(0.5f, 0.5f, 0.5f, 1.0f, 0.0f, 0.0f);
    EXPECT_FALSE(result.success);
}

TEST_F(VoxelInteractionTest, DestroyMatterAtNoHitReturnsFail) {
    VoxelInteraction vi(grid, dispatcher);
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
    VoxelInteraction vi(grid, dispatcher);
    VoxelHit hit{-5, -3, -7, 0, 0, -1, 1.0f};
    auto result = vi.createMatter(hit);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.x, -5);
    EXPECT_EQ(result.y, -3);
    EXPECT_EQ(result.z, -8);
    EXPECT_NE(grid.readCell(-5, -3, -8).materialId, material_ids::AIR);
}

TEST_F(VoxelInteractionTest, ChunkCoordsCalculatedCorrectly) {
    VoxelInteraction vi(grid, dispatcher);
    VoxelHit hit{31, 0, 0, 1, 0, 0, 1.0f}; // normal +X, target = (32, 0, 0)
    auto result = vi.createMatter(hit);
    EXPECT_EQ(result.x, 32);
    EXPECT_EQ(result.cx, 1); // 32 >> 5 = 1
    EXPECT_EQ(result.cy, 0);
    EXPECT_EQ(result.cz, 0);
}
