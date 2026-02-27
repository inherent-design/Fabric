#include "fabric/core/Ragdoll.hh"

#include <gtest/gtest.h>

#include <cstring>

using namespace fabric;

namespace {

void buildBindPose(float* matrices, int count) {
    std::memset(matrices, 0, static_cast<size_t>(count) * 16 * sizeof(float));
    for (int i = 0; i < count; ++i) {
        float* m = matrices + i * 16;
        m[0] = 1.0f;
        m[5] = 1.0f;
        m[10] = 1.0f;
        m[15] = 1.0f;
        m[13] = static_cast<float>(i); // translation Y
    }
}

class RagdollTest : public ::testing::Test {
  protected:
    void SetUp() override {
        physics_.init(4096, 1);
        ragdoll_.init(&physics_);
    }

    void TearDown() override {
        ragdoll_.shutdown();
        physics_.shutdown();
    }

    PhysicsWorld physics_;
    Ragdoll ragdoll_;
};

} // namespace

TEST_F(RagdollTest, InitAndShutdown) {
    EXPECT_EQ(ragdoll_.ragdollCount(), 0u);
}

TEST_F(RagdollTest, CreateRagdoll) {
    float matrices[5 * 16];
    buildBindPose(matrices, 5);

    RagdollHandle h = ragdoll_.createRagdoll(5, matrices);
    EXPECT_TRUE(h.valid());
    EXPECT_EQ(ragdoll_.ragdollCount(), 1u);
}

TEST_F(RagdollTest, DestroyRagdoll) {
    float matrices[5 * 16];
    buildBindPose(matrices, 5);

    RagdollHandle h = ragdoll_.createRagdoll(5, matrices);
    EXPECT_EQ(ragdoll_.ragdollCount(), 1u);

    ragdoll_.destroyRagdoll(h);
    EXPECT_EQ(ragdoll_.ragdollCount(), 0u);
}

TEST_F(RagdollTest, ActivateDeactivate) {
    float matrices[3 * 16];
    buildBindPose(matrices, 3);

    RagdollHandle h = ragdoll_.createRagdoll(3, matrices);
    EXPECT_FALSE(ragdoll_.isActive(h));

    ragdoll_.activate(h);
    EXPECT_TRUE(ragdoll_.isActive(h));

    ragdoll_.deactivate(h);
    EXPECT_FALSE(ragdoll_.isActive(h));
}

TEST_F(RagdollTest, GetJointTransforms) {
    float matrices[3 * 16];
    buildBindPose(matrices, 3);

    RagdollHandle h = ragdoll_.createRagdoll(3, matrices);

    float out[3 * 16];
    std::memset(out, 0, sizeof(out));
    ragdoll_.getJointTransforms(h, out, 3);

    for (int i = 0; i < 3; ++i) {
        float* m = out + i * 16;
        EXPECT_NEAR(m[13], static_cast<float>(i), 0.1f);
        EXPECT_FLOAT_EQ(m[15], 1.0f);
        // Identity rotation: diagonal should be ~1
        EXPECT_NEAR(m[0], 1.0f, 0.01f);
        EXPECT_NEAR(m[5], 1.0f, 0.01f);
        EXPECT_NEAR(m[10], 1.0f, 0.01f);
    }
}

TEST_F(RagdollTest, MultipleRagdolls) {
    float matrices[3 * 16];
    buildBindPose(matrices, 3);

    RagdollHandle h1 = ragdoll_.createRagdoll(3, matrices);
    RagdollHandle h2 = ragdoll_.createRagdoll(3, matrices);
    RagdollHandle h3 = ragdoll_.createRagdoll(3, matrices);

    EXPECT_TRUE(h1.valid());
    EXPECT_TRUE(h2.valid());
    EXPECT_TRUE(h3.valid());
    EXPECT_EQ(ragdoll_.ragdollCount(), 3u);
}

TEST_F(RagdollTest, DestroyInvalidHandle) {
    RagdollHandle invalid{0};
    ragdoll_.destroyRagdoll(invalid);
    EXPECT_EQ(ragdoll_.ragdollCount(), 0u);

    RagdollHandle bogus{9999};
    ragdoll_.destroyRagdoll(bogus);
    EXPECT_EQ(ragdoll_.ragdollCount(), 0u);
}

TEST_F(RagdollTest, JointCount) {
    float matrices5[5 * 16];
    buildBindPose(matrices5, 5);

    float matrices3[3 * 16];
    buildBindPose(matrices3, 3);

    RagdollHandle h5 = ragdoll_.createRagdoll(5, matrices5);
    RagdollHandle h3 = ragdoll_.createRagdoll(3, matrices3);

    EXPECT_EQ(ragdoll_.jointCount(h5), 5);
    EXPECT_EQ(ragdoll_.jointCount(h3), 3);
}
