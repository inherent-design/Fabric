#include "fabric/core/VoxelInteraction.hh"
#include "fabric/core/ChunkMeshManager.hh"
#include <gtest/gtest.h>

using namespace fabric;

class VoxelInteractionTest : public ::testing::Test {
  protected:
    DensityField density;
    EssenceField essence;
    EventDispatcher dispatcher;

    void SetUp() override {
        eventCount = 0;
        dispatcher.addEventListener(kVoxelChangedEvent, [this](Event&) { ++eventCount; });
    }

    int eventCount = 0;
};

TEST_F(VoxelInteractionTest, CreateMatterPlacesAdjacentPosZ) {
    VoxelInteraction vi(density, essence, dispatcher);
    VoxelHit hit{5, 5, 5, 0, 0, 1, 1.0f}; // normal +Z
    auto result = vi.createMatter(hit);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.x, 5);
    EXPECT_EQ(result.y, 5);
    EXPECT_EQ(result.z, 6);
    EXPECT_FLOAT_EQ(density.read(5, 5, 6), 1.0f);
}

TEST_F(VoxelInteractionTest, CreateMatterPlacesAdjacentNegZ) {
    VoxelInteraction vi(density, essence, dispatcher);
    VoxelHit hit{5, 5, 5, 0, 0, -1, 1.0f};
    auto result = vi.createMatter(hit);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.z, 4);
    EXPECT_FLOAT_EQ(density.read(5, 5, 4), 1.0f);
}

TEST_F(VoxelInteractionTest, CreateMatterPlacesAdjacentPosX) {
    VoxelInteraction vi(density, essence, dispatcher);
    VoxelHit hit{5, 5, 5, 1, 0, 0, 1.0f};
    auto result = vi.createMatter(hit);
    EXPECT_EQ(result.x, 6);
    EXPECT_FLOAT_EQ(density.read(6, 5, 5), 1.0f);
}

TEST_F(VoxelInteractionTest, CreateMatterPlacesAdjacentNegX) {
    VoxelInteraction vi(density, essence, dispatcher);
    VoxelHit hit{5, 5, 5, -1, 0, 0, 1.0f};
    auto result = vi.createMatter(hit);
    EXPECT_EQ(result.x, 4);
}

TEST_F(VoxelInteractionTest, CreateMatterPlacesAdjacentPosY) {
    VoxelInteraction vi(density, essence, dispatcher);
    VoxelHit hit{5, 5, 5, 0, 1, 0, 1.0f};
    auto result = vi.createMatter(hit);
    EXPECT_EQ(result.y, 6);
}

TEST_F(VoxelInteractionTest, CreateMatterPlacesAdjacentNegY) {
    VoxelInteraction vi(density, essence, dispatcher);
    VoxelHit hit{5, 5, 5, 0, -1, 0, 1.0f};
    auto result = vi.createMatter(hit);
    EXPECT_EQ(result.y, 4);
}

TEST_F(VoxelInteractionTest, CreateMatterWritesEssence) {
    VoxelInteraction vi(density, essence, dispatcher);
    Vector4<float, Space::World> color(1.0f, 0.0f, 0.0f, 1.0f);
    VoxelHit hit{5, 5, 5, 0, 0, 1, 1.0f};
    vi.createMatter(hit, 1.0f, color);
    auto stored = essence.read(5, 5, 6);
    EXPECT_FLOAT_EQ(stored.x, 1.0f);
    EXPECT_FLOAT_EQ(stored.y, 0.0f);
    EXPECT_FLOAT_EQ(stored.z, 0.0f);
    EXPECT_FLOAT_EQ(stored.w, 1.0f);
}

TEST_F(VoxelInteractionTest, DestroyMatterSetsDensityToZero) {
    VoxelInteraction vi(density, essence, dispatcher);
    density.write(5, 5, 5, 1.0f);
    VoxelHit hit{5, 5, 5, 0, 0, -1, 1.0f};
    auto result = vi.destroyMatter(hit);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.x, 5);
    EXPECT_FLOAT_EQ(density.read(5, 5, 5), 0.0f);
}

TEST_F(VoxelInteractionTest, CreateMatterEmitsVoxelChangedEvent) {
    VoxelInteraction vi(density, essence, dispatcher);
    VoxelHit hit{5, 5, 5, 0, 0, 1, 1.0f};
    vi.createMatter(hit);
    EXPECT_EQ(eventCount, 1);
}

TEST_F(VoxelInteractionTest, DestroyMatterEmitsVoxelChangedEvent) {
    VoxelInteraction vi(density, essence, dispatcher);
    density.write(5, 5, 5, 1.0f);
    VoxelHit hit{5, 5, 5, 0, 0, -1, 1.0f};
    vi.destroyMatter(hit);
    EXPECT_EQ(eventCount, 1);
}

TEST_F(VoxelInteractionTest, CreateMatterAtWithRaycast) {
    VoxelInteraction vi(density, essence, dispatcher);
    density.write(5, 5, 5, 1.0f);
    auto result = vi.createMatterAt(density.grid(), 5.5f, 5.5f, 0.5f, 0.0f, 0.0f, 1.0f);
    EXPECT_TRUE(result.success);
    // Ray hits +Z face of voxel at (5,5,5), normal is (0,0,-1), so placement at (5,5,4)
    EXPECT_EQ(result.z, 4);
    EXPECT_FLOAT_EQ(density.read(5, 5, 4), 1.0f);
}

TEST_F(VoxelInteractionTest, DestroyMatterAtWithRaycast) {
    VoxelInteraction vi(density, essence, dispatcher);
    density.write(5, 5, 5, 1.0f);
    auto result = vi.destroyMatterAt(density.grid(), 5.5f, 5.5f, 0.5f, 0.0f, 0.0f, 1.0f);
    EXPECT_TRUE(result.success);
    EXPECT_FLOAT_EQ(density.read(5, 5, 5), 0.0f);
}

TEST_F(VoxelInteractionTest, CreateMatterAtNoHitReturnsFail) {
    VoxelInteraction vi(density, essence, dispatcher);
    auto result = vi.createMatterAt(density.grid(), 0.5f, 0.5f, 0.5f, 1.0f, 0.0f, 0.0f);
    EXPECT_FALSE(result.success);
}

TEST_F(VoxelInteractionTest, DestroyMatterAtNoHitReturnsFail) {
    VoxelInteraction vi(density, essence, dispatcher);
    auto result = vi.destroyMatterAt(density.grid(), 0.5f, 0.5f, 0.5f, 1.0f, 0.0f, 0.0f);
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
    VoxelInteraction vi(density, essence, dispatcher);
    density.write(-5, -3, -7, 1.0f);
    VoxelHit hit{-5, -3, -7, 0, 0, -1, 1.0f};
    auto result = vi.createMatter(hit);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.x, -5);
    EXPECT_EQ(result.y, -3);
    EXPECT_EQ(result.z, -8);
    EXPECT_FLOAT_EQ(density.read(-5, -3, -8), 1.0f);
}

TEST_F(VoxelInteractionTest, ChunkCoordsCalculatedCorrectly) {
    VoxelInteraction vi(density, essence, dispatcher);
    VoxelHit hit{31, 0, 0, 1, 0, 0, 1.0f}; // normal +X, target = (32, 0, 0)
    auto result = vi.createMatter(hit);
    EXPECT_EQ(result.x, 32);
    EXPECT_EQ(result.cx, 1); // 32 >> 5 = 1
    EXPECT_EQ(result.cy, 0);
    EXPECT_EQ(result.cz, 0);
}
