#include <gtest/gtest.h>

#include "fabric/core/FieldLayer.hh"
#include "fabric/core/VoxelMesher.hh"

using namespace fabric;

class WaterMeshTest : public ::testing::Test {
  protected:
    FieldLayer<float> waterField;
    ChunkedGrid<float> density;
};

TEST_F(WaterMeshTest, DryCellsProduceNoGeometry) {
    auto data = VoxelMesher::meshWaterChunkData(0, 0, 0, waterField, density);
    EXPECT_EQ(data.vertices.size(), 0u);
    EXPECT_EQ(data.indices.size(), 0u);
}

TEST_F(WaterMeshTest, SingleWaterCellProducesExposedFaces) {
    waterField.write(0, 0, 0, 0.8f);
    auto data = VoxelMesher::meshWaterChunkData(0, 0, 0, waterField, density);
    // Isolated water cell: all 6 faces exposed
    EXPECT_EQ(data.vertices.size(), 24u);
    EXPECT_EQ(data.indices.size(), 36u);
}

TEST_F(WaterMeshTest, TopFaceHeightVariesWithFillLevel) {
    waterField.write(0, 0, 0, 0.5f);
    auto data = VoxelMesher::meshWaterChunkData(0, 0, 0, waterField, density);
    ASSERT_GT(data.vertices.size(), 0u);

    // Find top face vertices (normalIndex == 2 for +Y)
    uint8_t topY = 0;
    bool foundTop = false;
    for (const auto& v : data.vertices) {
        if (v.normalIndex() == 2) {
            topY = v.posY();
            foundTop = true;
            break;
        }
    }
    ASSERT_TRUE(foundTop);
    // Half-filled cell at y=0: top Y should be between 0 and 1
    EXPECT_GT(topY, 0u);
    EXPECT_LE(topY, 1u);
}

TEST_F(WaterMeshTest, FullCellTopFaceAtCellTop) {
    waterField.write(0, 0, 0, 1.0f);
    auto data = VoxelMesher::meshWaterChunkData(0, 0, 0, waterField, density);
    ASSERT_GT(data.vertices.size(), 0u);

    for (const auto& v : data.vertices) {
        if (v.normalIndex() == 2) {
            EXPECT_EQ(v.posY(), 1u);
        }
    }
}

TEST_F(WaterMeshTest, NoFacesBetweenSameLevelAdjacentCells) {
    waterField.write(0, 0, 0, 0.8f);
    waterField.write(1, 0, 0, 0.8f);
    auto data = VoxelMesher::meshWaterChunkData(0, 0, 0, waterField, density);
    // Each cell loses 1 shared face: 5 faces each = 10 total
    EXPECT_EQ(data.vertices.size(), 40u);
    EXPECT_EQ(data.indices.size(), 60u);
}

TEST_F(WaterMeshTest, FacesEmittedBetweenDifferentLevelCells) {
    waterField.write(0, 0, 0, 0.8f);
    waterField.write(1, 0, 0, 0.3f);
    auto data = VoxelMesher::meshWaterChunkData(0, 0, 0, waterField, density);
    // Different levels: shared face IS emitted for both cells
    EXPECT_EQ(data.vertices.size(), 48u);
    EXPECT_EQ(data.indices.size(), 72u);
}

TEST_F(WaterMeshTest, FlowEncodingFromNeighborDifferences) {
    waterField.write(5, 0, 5, 0.5f);
    waterField.write(4, 0, 5, 0.9f); // -X: higher
    waterField.write(6, 0, 5, 0.1f); // +X: lower

    auto data = VoxelMesher::meshWaterChunkData(0, 0, 0, waterField, density);
    ASSERT_GT(data.vertices.size(), 0u);

    // flowX = levelMx - levelPx = 0.9 - 0.1 = 0.8 -> positive
    // Adjacent cells emit separate quads with overlapping vertex positions.
    // Cell (4,0,5) processes before (5,0,5) in lx iteration, so take the
    // LAST matching vertex to get cell (5,0,5)'s flow values.
    int8_t lastFlowDx = 0;
    int8_t lastFlowDz = 0;
    bool foundCenterVertex = false;
    for (const auto& v : data.vertices) {
        if (v.posX() == 5 && v.posZ() == 6 && v.normalIndex() == 2) {
            lastFlowDx = v.flowDx;
            lastFlowDz = v.flowDz;
            foundCenterVertex = true;
        }
    }
    EXPECT_TRUE(foundCenterVertex);
    EXPECT_GT(lastFlowDx, 0);
    EXPECT_EQ(lastFlowDz, 0);
}

TEST_F(WaterMeshTest, SolidCellBlocksWaterMesh) {
    waterField.write(0, 0, 0, 1.0f);
    density.set(0, 0, 0, 1.0f);
    auto data = VoxelMesher::meshWaterChunkData(0, 0, 0, waterField, density);
    EXPECT_EQ(data.vertices.size(), 0u);
}

TEST_F(WaterMeshTest, WaterVertexSizeIs10Bytes) {
    EXPECT_EQ(sizeof(WaterVertex), 10u);
}

TEST_F(WaterMeshTest, AlphaFlagAlwaysSet) {
    waterField.write(0, 0, 0, 0.5f);
    auto data = VoxelMesher::meshWaterChunkData(0, 0, 0, waterField, density);
    EXPECT_TRUE(data.hasAlpha);
}

TEST_F(WaterMeshTest, SolidNeighborSuppressesFace) {
    waterField.write(0, 0, 0, 1.0f);
    density.set(1, 0, 0, 1.0f); // solid at +X
    auto data = VoxelMesher::meshWaterChunkData(0, 0, 0, waterField, density);
    // 5 visible faces (not +X which is against solid)
    EXPECT_EQ(data.vertices.size(), 20u);
    EXPECT_EQ(data.indices.size(), 30u);
}
