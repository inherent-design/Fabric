#include "fabric/core/Animation.hh"
#include <cmath>
#include <gtest/gtest.h>

using namespace fabric;

class AnimationTest : public ::testing::Test {};

TEST_F(AnimationTest, SkeletonComponentDefaultsToNull) {
    Skeleton skel;
    EXPECT_EQ(skel.skeleton, nullptr);
}

TEST_F(AnimationTest, AnimationClipComponentDefaultsToNull) {
    AnimationClip clip;
    EXPECT_EQ(clip.animation, nullptr);
    EXPECT_TRUE(clip.name.empty());
}

TEST_F(AnimationTest, AnimationStateDefaults) {
    AnimationState state;
    EXPECT_EQ(state.clip, nullptr);
    EXPECT_FLOAT_EQ(state.time, 0.0f);
    EXPECT_FLOAT_EQ(state.speed, 1.0f);
    EXPECT_TRUE(state.loop);
    EXPECT_TRUE(state.playing);
}

TEST_F(AnimationTest, BlendTreeCanHoldMultipleLayers) {
    AnimationBlendTree tree;
    tree.layers.resize(3);
    tree.layers[0].weight = 0.5f;
    tree.layers[1].weight = 0.3f;
    tree.layers[2].weight = 0.2f;
    EXPECT_EQ(tree.layers.size(), 3u);

    float totalWeight = 0.0f;
    for (const auto& layer : tree.layers) {
        totalWeight += layer.weight;
    }
    EXPECT_NEAR(totalWeight, 1.0f, 0.001f);
}

TEST_F(AnimationTest, SkinningDataJointMatrixCount) {
    SkinningData data;
    data.jointMatrices.resize(100);
    EXPECT_EQ(data.jointMatrices.size(), 100u);

    // Each matrix is 16 floats (4x4)
    EXPECT_EQ(data.jointMatrices[0].size(), 16u);
}

TEST_F(AnimationTest, OzzToFabricMatrixIdentityRoundTrip) {
    ozz::math::Float4x4 identity = ozz::math::Float4x4::identity();

    Matrix4x4<float> fabricMat = ozzToMatrix4x4(identity);

    // Check identity matrix values
    EXPECT_NEAR(fabricMat(0, 0), 1.0f, 1e-6f);
    EXPECT_NEAR(fabricMat(1, 1), 1.0f, 1e-6f);
    EXPECT_NEAR(fabricMat(2, 2), 1.0f, 1e-6f);
    EXPECT_NEAR(fabricMat(3, 3), 1.0f, 1e-6f);
    EXPECT_NEAR(fabricMat(0, 1), 0.0f, 1e-6f);
    EXPECT_NEAR(fabricMat(1, 0), 0.0f, 1e-6f);
}

TEST_F(AnimationTest, FabricToOzzMatrixIdentityRoundTrip) {
    Matrix4x4<float> identity; // default is identity

    ozz::math::Float4x4 ozzMat = matrix4x4ToOzz(identity);

    // Convert back
    Matrix4x4<float> roundTrip = ozzToMatrix4x4(ozzMat);

    for (int i = 0; i < 16; ++i) {
        EXPECT_NEAR(identity.elements[static_cast<size_t>(i)], roundTrip.elements[static_cast<size_t>(i)], 1e-6f)
            << "Mismatch at element " << i;
    }
}

TEST_F(AnimationTest, OzzToFabricMatrixTranslationPreserved) {
    ozz::math::Float4x4 mat = ozz::math::Float4x4::identity();
    // Set translation in column 3 (x=5, y=10, z=15)
    mat.cols[3] = ozz::math::simd_float4::Load(5.0f, 10.0f, 15.0f, 1.0f);

    Matrix4x4<float> fabricMat = ozzToMatrix4x4(mat);

    // Column-major: translation is at indices 12, 13, 14
    EXPECT_NEAR(fabricMat.elements[12], 5.0f, 1e-6f);
    EXPECT_NEAR(fabricMat.elements[13], 10.0f, 1e-6f);
    EXPECT_NEAR(fabricMat.elements[14], 15.0f, 1e-6f);
}

TEST_F(AnimationTest, MatrixConversionRoundTripArbitrary) {
    // Create an arbitrary rotation + translation matrix
    Matrix4x4<float> original;
    original.elements = {0.707f, 0.707f, 0.0f, 0.0f, -0.707f, 0.707f, 0.0f, 0.0f,
                         0.0f,   0.0f,   1.0f, 0.0f, 3.0f,    4.0f,   5.0f, 1.0f};

    ozz::math::Float4x4 ozzMat = matrix4x4ToOzz(original);
    Matrix4x4<float> roundTrip = ozzToMatrix4x4(ozzMat);

    for (int i = 0; i < 16; ++i) {
        EXPECT_NEAR(original.elements[static_cast<size_t>(i)], roundTrip.elements[static_cast<size_t>(i)], 1e-5f)
            << "Mismatch at element " << i;
    }
}

TEST_F(AnimationTest, MaxJointsConstant) {
    EXPECT_GE(kMaxJoints, 100) << "Must support at least 100 joints for humanoid characters";
}

TEST_F(AnimationTest, SkinningDataDefaultEmpty) {
    SkinningData data;
    EXPECT_TRUE(data.jointMatrices.empty());
}
