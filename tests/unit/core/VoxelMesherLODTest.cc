#include "fabric/core/VoxelMesher.hh"
#include <gtest/gtest.h>

using namespace fabric;
using Essence = Vector4<float, Space::World>;

class VoxelMesherLODTest : public ::testing::Test {
  protected:
    ChunkedGrid<float> density;
    ChunkedGrid<Essence> essence;

    void fillSolidChunk() {
        for (int z = 0; z < kChunkSize; ++z)
            for (int y = 0; y < kChunkSize; ++y)
                for (int x = 0; x < kChunkSize; ++x)
                    density.set(x, y, z, 1.0f);
    }

    // 3D checkerboard: every other voxel filled. Maximally complex surface
    // at LOD 0 (each voxel exposes up to 6 faces). At LOD 1, max-density
    // sampling over 2x2x2 cells fills all cells, collapsing to a solid block.
    void fillCheckerboard() {
        for (int z = 0; z < kChunkSize; ++z)
            for (int y = 0; y < kChunkSize; ++y)
                for (int x = 0; x < kChunkSize; ++x)
                    if ((x + y + z) % 2 == 0)
                        density.set(x, y, z, 1.0f);
    }
};

TEST_F(VoxelMesherLODTest, LOD0ByteIdenticalToDefault) {
    // Single voxel
    density.set(0, 0, 0, 1.0f);
    essence.set(0, 0, 0, Essence(0.5f, 0.3f, 0.7f, 0.1f));

    auto defaultData = VoxelMesher::meshChunkData(0, 0, 0, density, essence, 0.5f);
    auto lod0Data = VoxelMesher::meshChunkData(0, 0, 0, density, essence, 0.5f, 0);

    ASSERT_EQ(defaultData.vertices.size(), lod0Data.vertices.size());
    ASSERT_EQ(defaultData.indices.size(), lod0Data.indices.size());
    ASSERT_EQ(defaultData.palette.size(), lod0Data.palette.size());

    for (size_t i = 0; i < defaultData.vertices.size(); ++i) {
        EXPECT_EQ(defaultData.vertices[i].posNormalAO, lod0Data.vertices[i].posNormalAO)
            << "Vertex " << i << " posNormalAO mismatch";
        EXPECT_EQ(defaultData.vertices[i].material, lod0Data.vertices[i].material)
            << "Vertex " << i << " material mismatch";
    }

    for (size_t i = 0; i < defaultData.indices.size(); ++i) {
        EXPECT_EQ(defaultData.indices[i], lod0Data.indices[i]) << "Index " << i << " mismatch";
    }
}

TEST_F(VoxelMesherLODTest, LOD0SolidChunkByteIdentical) {
    fillSolidChunk();

    auto defaultData = VoxelMesher::meshChunkData(0, 0, 0, density, essence, 0.5f);
    auto lod0Data = VoxelMesher::meshChunkData(0, 0, 0, density, essence, 0.5f, 0);

    ASSERT_EQ(defaultData.vertices.size(), lod0Data.vertices.size());
    ASSERT_EQ(defaultData.indices.size(), lod0Data.indices.size());

    for (size_t i = 0; i < defaultData.vertices.size(); ++i) {
        EXPECT_EQ(defaultData.vertices[i].posNormalAO, lod0Data.vertices[i].posNormalAO);
        EXPECT_EQ(defaultData.vertices[i].material, lod0Data.vertices[i].material);
    }
}

TEST_F(VoxelMesherLODTest, LOD1CheckerboardReducesVertices) {
    // Checkerboard produces many faces at LOD 0 (each isolated voxel has up
    // to 6 exposed faces). LOD 1 max-density sampling over 2x2x2 cells fills
    // every cell, collapsing to a solid block with far fewer vertices.
    fillCheckerboard();

    auto lod0 = VoxelMesher::meshChunkData(0, 0, 0, density, essence, 0.5f, 0);
    auto lod1 = VoxelMesher::meshChunkData(0, 0, 0, density, essence, 0.5f, 1);

    ASSERT_GT(lod0.vertices.size(), 0u);
    ASSERT_GT(lod1.vertices.size(), 0u);
    // LOD 1 must produce significantly fewer vertices than LOD 0
    EXPECT_LT(lod1.vertices.size(), lod0.vertices.size());
}

TEST_F(VoxelMesherLODTest, LOD2CheckerboardReducesVertices) {
    fillCheckerboard();

    auto lod0 = VoxelMesher::meshChunkData(0, 0, 0, density, essence, 0.5f, 0);
    auto lod2 = VoxelMesher::meshChunkData(0, 0, 0, density, essence, 0.5f, 2);

    ASSERT_GT(lod0.vertices.size(), 0u);
    ASSERT_GT(lod2.vertices.size(), 0u);
    // LOD 2 must produce significantly fewer vertices than LOD 0
    EXPECT_LT(lod2.vertices.size(), lod0.vertices.size());
}

TEST_F(VoxelMesherLODTest, SolidChunkSameVerticesAllLODs) {
    // A solid uniform chunk is a single box at any LOD level; the greedy
    // mesher produces 6 quads (24 vertices) regardless of grid resolution.
    fillSolidChunk();

    auto lod0 = VoxelMesher::meshChunkData(0, 0, 0, density, essence, 0.5f, 0);
    auto lod1 = VoxelMesher::meshChunkData(0, 0, 0, density, essence, 0.5f, 1);
    auto lod2 = VoxelMesher::meshChunkData(0, 0, 0, density, essence, 0.5f, 2);

    EXPECT_EQ(lod0.vertices.size(), lod1.vertices.size());
    EXPECT_EQ(lod0.vertices.size(), lod2.vertices.size());
}

TEST_F(VoxelMesherLODTest, VoxelVertexStructSize) {
    static_assert(sizeof(VoxelVertex) == 8, "VoxelVertex must be 8 bytes");
    EXPECT_EQ(sizeof(VoxelVertex), 8u);
}

TEST_F(VoxelMesherLODTest, PositionsInBounds) {
    fillSolidChunk();

    for (int lod = 0; lod <= 2; ++lod) {
        auto data = VoxelMesher::meshChunkData(0, 0, 0, density, essence, 0.5f, lod);
        for (size_t i = 0; i < data.vertices.size(); ++i) {
            EXPECT_LE(data.vertices[i].posX(), kChunkSize) << "LOD " << lod << " vertex " << i << " posX out of range";
            EXPECT_LE(data.vertices[i].posY(), kChunkSize) << "LOD " << lod << " vertex " << i << " posY out of range";
            EXPECT_LE(data.vertices[i].posZ(), kChunkSize) << "LOD " << lod << " vertex " << i << " posZ out of range";
        }
    }
}

TEST_F(VoxelMesherLODTest, LOD1EmptyChunkStaysEmpty) {
    auto data = VoxelMesher::meshChunkData(0, 0, 0, density, essence, 0.5f, 1);
    EXPECT_EQ(data.vertices.size(), 0u);
    EXPECT_EQ(data.indices.size(), 0u);
}

TEST_F(VoxelMesherLODTest, LOD1SingleVoxelStillVisible) {
    // A single voxel at (0,0,0) should be captured by LOD 1 sampling
    density.set(0, 0, 0, 1.0f);
    auto data = VoxelMesher::meshChunkData(0, 0, 0, density, essence, 0.5f, 1);
    EXPECT_GT(data.vertices.size(), 0u);
}

TEST_F(VoxelMesherLODTest, LOD1PositionsAreStrideAligned) {
    fillSolidChunk();
    auto data = VoxelMesher::meshChunkData(0, 0, 0, density, essence, 0.5f, 1);

    for (size_t i = 0; i < data.vertices.size(); ++i) {
        EXPECT_EQ(data.vertices[i].posX() % 2, 0u) << "LOD 1 vertex " << i << " posX not stride-aligned";
        EXPECT_EQ(data.vertices[i].posY() % 2, 0u) << "LOD 1 vertex " << i << " posY not stride-aligned";
        EXPECT_EQ(data.vertices[i].posZ() % 2, 0u) << "LOD 1 vertex " << i << " posZ not stride-aligned";
    }
}
