#include "recurse/animation/SkinnedRenderer.hh"
#include <gtest/gtest.h>

using namespace fabric;
using namespace recurse;

class SkinnedRendererTest : public ::testing::Test {};

TEST_F(SkinnedRendererTest, VertexLayoutStride) {
    auto layout = createSkinnedVertexLayout();
    // pos(3*4=12) + normal(3*4=12) + uv(2*4=8) + joints(4*1=4) + weights(4*4=16) = 52 bytes
    EXPECT_EQ(layout.getStride(), 52u);
}

TEST_F(SkinnedRendererTest, VertexLayoutHasPositionAttribute) {
    auto layout = createSkinnedVertexLayout();
    EXPECT_TRUE(layout.has(bgfx::Attrib::Position));
}

TEST_F(SkinnedRendererTest, VertexLayoutHasNormalAttribute) {
    auto layout = createSkinnedVertexLayout();
    EXPECT_TRUE(layout.has(bgfx::Attrib::Normal));
}

TEST_F(SkinnedRendererTest, VertexLayoutHasTexCoordAttribute) {
    auto layout = createSkinnedVertexLayout();
    EXPECT_TRUE(layout.has(bgfx::Attrib::TexCoord0));
}

TEST_F(SkinnedRendererTest, VertexLayoutHasIndicesAttribute) {
    auto layout = createSkinnedVertexLayout();
    EXPECT_TRUE(layout.has(bgfx::Attrib::Indices));
}

TEST_F(SkinnedRendererTest, VertexLayoutHasWeightAttribute) {
    auto layout = createSkinnedVertexLayout();
    EXPECT_TRUE(layout.has(bgfx::Attrib::Weight));
}

TEST_F(SkinnedRendererTest, MaxGpuJointsConstant) {
    EXPECT_GE(K_MAX_GPU_JOINTS, 60) << "Must support at least 60 joints for humanoid characters";
    EXPECT_LE(K_MAX_GPU_JOINTS, 128) << "GPU uniform arrays have practical limits";
}

TEST_F(SkinnedRendererTest, SkinningDataCanHoldMaxJoints) {
    SkinningData data;
    data.jointMatrices.resize(K_MAX_GPU_JOINTS);
    EXPECT_EQ(data.jointMatrices.size(), static_cast<size_t>(K_MAX_GPU_JOINTS));
}

TEST_F(SkinnedRendererTest, MeshDataHasStableId) {
    MeshData a;
    MeshData b;
    EXPECT_NE(a.id, b.id) << "Each MeshData must get a unique cache key";
    EXPECT_NE(a.id, 0u);
    EXPECT_NE(b.id, 0u);
}

TEST_F(SkinnedRendererTest, MeshBufferCacheKeyIsUint64) {
    // Verify the cache uses uint64_t by checking MeshData.id type
    MeshData mesh;
    static_assert(std::is_same_v<decltype(mesh.id), uint64_t>, "MeshData.id must be uint64_t for cache keying");
}
