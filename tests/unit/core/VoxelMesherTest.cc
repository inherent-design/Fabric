#include <gtest/gtest.h>
#include "fabric/core/VoxelMesher.hh"

using namespace fabric;
using Essence = Vector4<float, Space::World>;

class VoxelMesherTest : public ::testing::Test {
protected:
    ChunkedGrid<float> density;
    ChunkedGrid<Essence> essence;
};

TEST_F(VoxelMesherTest, EmptyChunkProducesNoGeometry) {
    // No solid cells set, chunk 0,0,0 should produce nothing
    auto data = VoxelMesher::meshChunkData(0, 0, 0, density, essence);
    EXPECT_EQ(data.vertices.size(), 0u);
    EXPECT_EQ(data.indices.size(), 0u);
}

TEST_F(VoxelMesherTest, SingleSolidCellProducesSixFaces) {
    density.set(0, 0, 0, 1.0f);
    auto data = VoxelMesher::meshChunkData(0, 0, 0, density, essence);
    // 6 faces * 4 verts = 24
    EXPECT_EQ(data.vertices.size(), 24u);
    // 6 faces * 6 indices = 36
    EXPECT_EQ(data.indices.size(), 36u);
}

TEST_F(VoxelMesherTest, TwoAdjacentCellsCullSharedFace) {
    // Two cells adjacent along X: shared +X/-X face is culled
    density.set(0, 0, 0, 1.0f);
    density.set(1, 0, 0, 1.0f);
    auto data = VoxelMesher::meshChunkData(0, 0, 0, density, essence);
    // 2 cells * 6 faces - 2 shared faces = 10 faces
    // 10 * 4 = 40 verts, 10 * 6 = 60 indices
    EXPECT_EQ(data.vertices.size(), 40u);
    EXPECT_EQ(data.indices.size(), 60u);
}

TEST_F(VoxelMesherTest, Solid2x2x2BlockExposedFaces) {
    for (int z = 0; z < 2; ++z)
        for (int y = 0; y < 2; ++y)
            for (int x = 0; x < 2; ++x)
                density.set(x, y, z, 1.0f);

    auto data = VoxelMesher::meshChunkData(0, 0, 0, density, essence);
    // A 2x2x2 cube has 24 exposed faces (4 per axis direction * 2 directions * 3 axes)
    // Each face of the outer surface: 2x2 = 4 faces per side, 6 sides = 24 faces
    EXPECT_EQ(data.vertices.size(), 24u * 4);  // 96
    EXPECT_EQ(data.indices.size(), 24u * 6);    // 144
}

TEST_F(VoxelMesherTest, NormalsAreCorrect) {
    density.set(0, 0, 0, 1.0f);
    auto data = VoxelMesher::meshChunkData(0, 0, 0, density, essence);

    // Collect unique normals from all faces
    // Each face has 4 verts with the same normal
    bool foundPosX = false, foundNegX = false;
    bool foundPosY = false, foundNegY = false;
    bool foundPosZ = false, foundNegZ = false;

    for (size_t i = 0; i < data.vertices.size(); i += 4) {
        float nx = data.vertices[i].nx;
        float ny = data.vertices[i].ny;
        float nz = data.vertices[i].nz;
        if (nx ==  1.0f && ny == 0.0f && nz == 0.0f) foundPosX = true;
        if (nx == -1.0f && ny == 0.0f && nz == 0.0f) foundNegX = true;
        if (nx == 0.0f && ny ==  1.0f && nz == 0.0f) foundPosY = true;
        if (nx == 0.0f && ny == -1.0f && nz == 0.0f) foundNegY = true;
        if (nx == 0.0f && ny == 0.0f && nz ==  1.0f) foundPosZ = true;
        if (nx == 0.0f && ny == 0.0f && nz == -1.0f) foundNegZ = true;
    }

    EXPECT_TRUE(foundPosX);
    EXPECT_TRUE(foundNegX);
    EXPECT_TRUE(foundPosY);
    EXPECT_TRUE(foundNegY);
    EXPECT_TRUE(foundPosZ);
    EXPECT_TRUE(foundNegZ);
}

TEST_F(VoxelMesherTest, EssenceToColorMapping) {
    density.set(0, 0, 0, 1.0f);
    // essence = [Order=0, Chaos=1, Life=0, Decay=0]
    essence.set(0, 0, 0, Essence(0.0f, 1.0f, 0.0f, 0.0f));

    auto data = VoxelMesher::meshChunkData(0, 0, 0, density, essence);
    ASSERT_GT(data.vertices.size(), 0u);

    // R = Chaos = 1.0, G = Life = 0.0, B = Order = 0.0, A = 1.0
    auto& v = data.vertices[0];
    EXPECT_FLOAT_EQ(v.r, 1.0f);
    EXPECT_FLOAT_EQ(v.g, 0.0f);
    EXPECT_FLOAT_EQ(v.b, 0.0f);
    EXPECT_FLOAT_EQ(v.a, 1.0f);
}

TEST_F(VoxelMesherTest, ZeroEssenceUsesDefaultGray) {
    density.set(0, 0, 0, 1.0f);
    // essence defaults to all zeros (not set)

    auto data = VoxelMesher::meshChunkData(0, 0, 0, density, essence);
    ASSERT_GT(data.vertices.size(), 0u);

    auto& v = data.vertices[0];
    EXPECT_FLOAT_EQ(v.r, 0.5f);
    EXPECT_FLOAT_EQ(v.g, 0.5f);
    EXPECT_FLOAT_EQ(v.b, 0.5f);
    EXPECT_FLOAT_EQ(v.a, 1.0f);
}

TEST_F(VoxelMesherTest, ThresholdExcludesLowDensity) {
    density.set(0, 0, 0, 0.3f);  // below default threshold 0.5
    auto data = VoxelMesher::meshChunkData(0, 0, 0, density, essence);
    EXPECT_EQ(data.vertices.size(), 0u);
    EXPECT_EQ(data.indices.size(), 0u);
}

TEST_F(VoxelMesherTest, DecayAffectsAlpha) {
    density.set(0, 0, 0, 1.0f);
    // essence = [Order=0.5, Chaos=0.3, Life=0.7, Decay=0.8]
    essence.set(0, 0, 0, Essence(0.5f, 0.3f, 0.7f, 0.8f));

    auto data = VoxelMesher::meshChunkData(0, 0, 0, density, essence);
    ASSERT_GT(data.vertices.size(), 0u);

    auto& v = data.vertices[0];
    EXPECT_FLOAT_EQ(v.r, 0.3f);   // Chaos
    EXPECT_FLOAT_EQ(v.g, 0.7f);   // Life
    EXPECT_FLOAT_EQ(v.b, 0.5f);   // Order
    EXPECT_FLOAT_EQ(v.a, 0.6f);   // 1.0 - 0.8*0.5
}
