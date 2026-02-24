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
    // 10 exposed faces, coplanar pairs merge into 6 quads
    EXPECT_EQ(data.vertices.size(), 24u);
    EXPECT_EQ(data.indices.size(), 36u);
}

TEST_F(VoxelMesherTest, Solid2x2x2BlockExposedFaces) {
    for (int z = 0; z < 2; ++z)
        for (int y = 0; y < 2; ++y)
            for (int x = 0; x < 2; ++x)
                density.set(x, y, z, 1.0f);

    auto data = VoxelMesher::meshChunkData(0, 0, 0, density, essence);
    // Each of the 6 cube faces (2x2) merges into 1 quad
    EXPECT_EQ(data.vertices.size(), 6u * 4);   // 24
    EXPECT_EQ(data.indices.size(), 6u * 6);     // 36
}

TEST_F(VoxelMesherTest, NormalsAreCorrect) {
    density.set(0, 0, 0, 1.0f);
    auto data = VoxelMesher::meshChunkData(0, 0, 0, density, essence);

    // Check all 6 face directions are present via normal index
    bool found[6] = {};
    for (size_t i = 0; i < data.vertices.size(); i += 4) {
        found[data.vertices[i].normalIndex()] = true;
    }

    for (int f = 0; f < 6; ++f) {
        EXPECT_TRUE(found[f]) << "Missing face direction " << f;
    }
}

TEST_F(VoxelMesherTest, EssenceToColorMapping) {
    density.set(0, 0, 0, 1.0f);
    // essence = [Order=0, Chaos=1, Life=0, Decay=0]
    essence.set(0, 0, 0, Essence(0.0f, 1.0f, 0.0f, 0.0f));

    auto data = VoxelMesher::meshChunkData(0, 0, 0, density, essence);
    ASSERT_GT(data.vertices.size(), 0u);
    ASSERT_GT(data.palette.size(), 0u);

    // R = Chaos = 1.0, G = Life = 0.0, B = Order = 0.0, A = 1.0
    auto& c = data.palette[data.vertices[0].paletteIndex()];
    EXPECT_FLOAT_EQ(c[0], 1.0f);
    EXPECT_FLOAT_EQ(c[1], 0.0f);
    EXPECT_FLOAT_EQ(c[2], 0.0f);
    EXPECT_FLOAT_EQ(c[3], 1.0f);
}

TEST_F(VoxelMesherTest, ZeroEssenceUsesDefaultGray) {
    density.set(0, 0, 0, 1.0f);
    // essence defaults to all zeros (not set)

    auto data = VoxelMesher::meshChunkData(0, 0, 0, density, essence);
    ASSERT_GT(data.vertices.size(), 0u);
    ASSERT_GT(data.palette.size(), 0u);

    auto& c = data.palette[data.vertices[0].paletteIndex()];
    EXPECT_FLOAT_EQ(c[0], 0.5f);
    EXPECT_FLOAT_EQ(c[1], 0.5f);
    EXPECT_FLOAT_EQ(c[2], 0.5f);
    EXPECT_FLOAT_EQ(c[3], 1.0f);
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
    ASSERT_GT(data.palette.size(), 0u);

    auto& c = data.palette[data.vertices[0].paletteIndex()];
    EXPECT_FLOAT_EQ(c[0], 0.3f);   // Chaos
    EXPECT_FLOAT_EQ(c[1], 0.7f);   // Life
    EXPECT_FLOAT_EQ(c[2], 0.5f);   // Order
    EXPECT_FLOAT_EQ(c[3], 0.6f);   // 1.0 - 0.8*0.5
}

TEST_F(VoxelMesherTest, GreedyMergesFlatWall) {
    for (int y = 0; y < 4; ++y)
        for (int x = 0; x < 4; ++x)
            density.set(x, y, 0, 1.0f);

    auto data = VoxelMesher::meshChunkData(0, 0, 0, density, essence);
    // 4x4x1 slab: each of the 6 faces merges to 1 quad
    EXPECT_EQ(data.vertices.size(), 24u);
    EXPECT_EQ(data.indices.size(), 36u);
}

TEST_F(VoxelMesherTest, GreedyMergesRowButNotMismatchedEssence) {
    Essence essA(1.0f, 0.0f, 0.0f, 0.0f);
    Essence essB(0.0f, 1.0f, 0.0f, 0.0f);

    for (int x = 0; x < 4; ++x) {
        density.set(x, 0, 0, 1.0f);
        essence.set(x, 0, 0, x < 2 ? essA : essB);
    }

    auto data = VoxelMesher::meshChunkData(0, 0, 0, density, essence);
    // +X, -X: 1 quad each; +Y,-Y,+Z,-Z: 2 quads each (split by essence)
    // Total: 2 + 4*2 = 10 quads
    EXPECT_EQ(data.vertices.size(), 40u);
    EXPECT_EQ(data.indices.size(), 60u);
}

TEST_F(VoxelMesherTest, GreedyFullChunkSingleMaterial) {
    for (int z = 0; z < kChunkSize; ++z)
        for (int y = 0; y < kChunkSize; ++y)
            for (int x = 0; x < kChunkSize; ++x)
                density.set(x, y, z, 1.0f);

    auto data = VoxelMesher::meshChunkData(0, 0, 0, density, essence);
    // Only 6 outer faces, each 32x32, each merges to 1 quad
    EXPECT_EQ(data.vertices.size(), 24u);
    EXPECT_EQ(data.indices.size(), 36u);
}

TEST_F(VoxelMesherTest, GreedyLShapePartialMerge) {
    // L-shape: 3x3 at z=0 missing top-right corner (2,2,0)
    for (int y = 0; y < 3; ++y)
        for (int x = 0; x < 3; ++x)
            if (!(x == 2 && y == 2))
                density.set(x, y, 0, 1.0f);

    auto data = VoxelMesher::meshChunkData(0, 0, 0, density, essence);
    // More quads than a full 3x3 (6 quads = 24 verts)
    EXPECT_GT(data.vertices.size(), 24u);
    // Fewer than fully unmerged (28 exposed faces = 112 verts)
    EXPECT_LT(data.vertices.size(), 112u);
}

TEST_F(VoxelMesherTest, AOIsolatedVoxelAllCornersExposed) {
    density.set(0, 0, 0, 1.0f);
    auto data = VoxelMesher::meshChunkData(0, 0, 0, density, essence);
    ASSERT_EQ(data.vertices.size(), 24u);

    for (const auto& v : data.vertices) {
        EXPECT_EQ(v.aoLevel(), 3u);
    }
}

TEST_F(VoxelMesherTest, AOCornerOccludedByTwoSides) {
    density.set(0, 0, 0, 1.0f);
    density.set(1, 0, 1, 1.0f);
    density.set(0, 1, 1, 1.0f);

    auto data = VoxelMesher::meshChunkData(0, 0, 0, density, essence);

    // On the +Z face of (0,0,0), vertex at (1,1,1) has both AO sides occupied
    bool found = false;
    for (const auto& v : data.vertices) {
        if (v.posX() == 1 && v.posY() == 1 && v.posZ() == 1 && v.normalIndex() == 4) {
            EXPECT_EQ(v.aoLevel(), 0u);
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
}

TEST_F(VoxelMesherTest, AOPartialOcclusion) {
    density.set(0, 0, 0, 1.0f);
    density.set(1, 0, 1, 1.0f);

    auto data = VoxelMesher::meshChunkData(0, 0, 0, density, essence);

    // On the +Z face of (0,0,0), vertex at (1,1,1): side1 solid, side2 empty, corner empty
    // ao level = 3 - 1 = 2
    bool found = false;
    for (const auto& v : data.vertices) {
        if (v.posX() == 1 && v.posY() == 1 && v.posZ() == 1 && v.normalIndex() == 4) {
            EXPECT_EQ(v.aoLevel(), 2u);
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
}

TEST_F(VoxelMesherTest, PackedVertexRoundTrip) {
    auto v = VoxelVertex::pack(17, 25, 3, 4, 2, 1023);
    EXPECT_EQ(v.posX(), 17);
    EXPECT_EQ(v.posY(), 25);
    EXPECT_EQ(v.posZ(), 3);
    EXPECT_EQ(v.normalIndex(), 4);
    EXPECT_EQ(v.aoLevel(), 2);
    EXPECT_EQ(v.paletteIndex(), 1023);
}

TEST_F(VoxelMesherTest, PackedVertexSizeIs8Bytes) {
    EXPECT_EQ(sizeof(VoxelVertex), 8u);
}

TEST_F(VoxelMesherTest, PaletteDeduplicatesColors) {
    // Two cells with same zero essence should share one palette entry
    density.set(0, 0, 0, 1.0f);
    density.set(2, 0, 0, 1.0f);

    auto data = VoxelMesher::meshChunkData(0, 0, 0, density, essence);
    EXPECT_EQ(data.palette.size(), 1u);
}
