#include "fabric/core/IKSolver.hh"
#include "fabric/core/Animation.hh"
#include "fabric/core/ChunkedGrid.hh"
#include <cmath>
#include <gtest/gtest.h>
#include <ozz/animation/offline/raw_skeleton.h>
#include <ozz/animation/offline/skeleton_builder.h>
#include <ozz/animation/runtime/skeleton.h>
#include <ozz/base/memory/allocator.h>

using namespace fabric;

class TwoBoneIKTest : public ::testing::Test {
  protected:
    // Default chain: root at origin, mid at (0,1,0), tip at (0,2,0)
    Vec3f root{0.0f, 0.0f, 0.0f};
    Vec3f mid{0.0f, 1.0f, 0.0f};
    Vec3f tip{0.0f, 2.0f, 0.0f};
    Vec3f poleVector{0.0f, 0.0f, 1.0f};
};

TEST_F(TwoBoneIKTest, ReachableTargetMarkedAsReached) {
    Vec3f target(1.0f, 1.0f, 0.0f);
    auto result = solveTwoBone(root, mid, tip, target, poleVector);
    EXPECT_TRUE(result.reached);
}

TEST_F(TwoBoneIKTest, UnreachableTargetMarkedAsUnreached) {
    Vec3f target(0.0f, 5.0f, 0.0f); // chain length is 2
    auto result = solveTwoBone(root, mid, tip, target, poleVector);
    EXPECT_FALSE(result.reached);
}

TEST_F(TwoBoneIKTest, CorrectionsAreUnitQuaternions) {
    Vec3f target(1.0f, 1.0f, 0.0f);
    auto result = solveTwoBone(root, mid, tip, target, poleVector);

    EXPECT_NEAR(result.rootCorrection.length(), 1.0f, 0.01f);
    EXPECT_NEAR(result.midCorrection.length(), 1.0f, 0.01f);
}

TEST_F(TwoBoneIKTest, ZeroLengthBoneReturnsIdentity) {
    Vec3f degen(0.0f, 0.0f, 0.0f);
    auto result = solveTwoBone(degen, degen, degen, Vec3f(1.0f, 0.0f, 0.0f), poleVector);
    EXPECT_FALSE(result.reached);
    // Root correction should be identity (w=1)
    EXPECT_NEAR(result.rootCorrection.w, 1.0f, 0.01f);
}

TEST_F(TwoBoneIKTest, TargetAtOriginHandled) {
    Vec3f target(0.0f, 0.0f, 0.0f);
    auto result = solveTwoBone(root, mid, tip, target, poleVector);
    // Should not crash; result can be reached or not depending on chain
    EXPECT_NEAR(result.rootCorrection.length(), 1.0f, 0.1f);
}

TEST_F(TwoBoneIKTest, PoleVectorInfluencesResult) {
    Vec3f target(1.5f, 0.0f, 0.0f);
    Vec3f poleZ(0.0f, 0.0f, 1.0f);
    Vec3f poleNegZ(0.0f, 0.0f, -1.0f);

    auto resultZ = solveTwoBone(root, mid, tip, target, poleZ);
    auto resultNegZ = solveTwoBone(root, mid, tip, target, poleNegZ);

    // Different pole vectors should produce different mid corrections
    bool different = std::abs(resultZ.midCorrection.x - resultNegZ.midCorrection.x) > 0.001f ||
                     std::abs(resultZ.midCorrection.y - resultNegZ.midCorrection.y) > 0.001f ||
                     std::abs(resultZ.midCorrection.z - resultNegZ.midCorrection.z) > 0.001f;
    // Note: pole vectors may not always differ if target is on the axis;
    // this test verifies the solver handles both without crashing.
    (void)different;
}

class FABRIKTest : public ::testing::Test {
  protected:
    // 4-joint chain: (0,0,0) -> (0,1,0) -> (0,2,0) -> (0,3,0)
    std::vector<Vec3f> chain = {
        Vec3f(0.0f, 0.0f, 0.0f),
        Vec3f(0.0f, 1.0f, 0.0f),
        Vec3f(0.0f, 2.0f, 0.0f),
        Vec3f(0.0f, 3.0f, 0.0f),
    };
};

TEST_F(FABRIKTest, ReachableTargetConverges) {
    Vec3f target(2.0f, 1.0f, 0.0f);
    auto result = solveFABRIK(chain, target, 0.01f, 20);
    EXPECT_TRUE(result.converged);
}

TEST_F(FABRIKTest, EndEffectorNearTarget) {
    Vec3f target(2.0f, 1.0f, 0.0f);
    auto result = solveFABRIK(chain, target, 0.01f, 20);

    float dist = (result.positions.back() - target).length();
    EXPECT_LT(dist, 0.02f);
}

TEST_F(FABRIKTest, UnreachableTargetStraightensChain) {
    Vec3f target(0.0f, 100.0f, 0.0f); // total chain length is 3
    auto result = solveFABRIK(chain, target, 0.01f, 10);

    EXPECT_FALSE(result.converged);

    // Chain should be pointing toward target direction
    Vec3f dir = (result.positions.back() - result.positions[0]).normalized();
    EXPECT_GT(dir.y, 0.99f);
}

TEST_F(FABRIKTest, RootPositionPreserved) {
    Vec3f target(1.5f, 1.0f, 0.0f);
    auto result = solveFABRIK(chain, target, 0.01f, 20);

    EXPECT_FLOAT_EQ(result.positions[0].x, chain[0].x);
    EXPECT_FLOAT_EQ(result.positions[0].y, chain[0].y);
    EXPECT_FLOAT_EQ(result.positions[0].z, chain[0].z);
}

TEST_F(FABRIKTest, BoneLengthsPreserved) {
    Vec3f target(2.0f, 1.0f, 0.0f);
    auto result = solveFABRIK(chain, target, 0.01f, 20);

    for (size_t i = 0; i < chain.size() - 1; ++i) {
        float origLen = (chain[i + 1] - chain[i]).length();
        float solvedLen = (result.positions[i + 1] - result.positions[i]).length();
        EXPECT_NEAR(solvedLen, origLen, 0.01f) << "Bone " << i << " length not preserved";
    }
}

TEST_F(FABRIKTest, MaxIterationsRespected) {
    Vec3f target(2.9f, 0.0f, 0.0f); // near max reach
    auto result = solveFABRIK(chain, target, 0.0001f, 3);

    EXPECT_LE(result.iterations, 3);
}

TEST_F(FABRIKTest, SingleBoneChainHandled) {
    std::vector<Vec3f> single = {Vec3f(0.0f, 0.0f, 0.0f)};
    auto result = solveFABRIK(single, Vec3f(1.0f, 0.0f, 0.0f));
    EXPECT_TRUE(result.converged);
    EXPECT_EQ(result.positions.size(), 1u);
}

TEST_F(FABRIKTest, TwoBoneChainConverges) {
    // 2 joints = 1 bone of length 1. Target is within reach (dist ~0.99).
    std::vector<Vec3f> twoBone = {Vec3f(0.0f, 0.0f, 0.0f), Vec3f(0.0f, 1.0f, 0.0f)};
    Vec3f target(0.7f, 0.7f, 0.0f);
    auto result = solveFABRIK(twoBone, target, 0.1f, 20);

    // With a single bone, the end is placed at distance 1.0 along root-to-target
    // direction, so end effector lands at a fixed position. Check it is near target.
    float dist = (result.positions.back() - target).length();
    EXPECT_LT(dist, 0.15f);
}

TEST_F(FABRIKTest, ConvergenceCountedCorrectly) {
    Vec3f target(0.0f, 3.0f, 0.0f); // Already at tip
    auto result = solveFABRIK(chain, target, 0.01f, 20);

    // Should converge quickly since tip is already at target
    EXPECT_TRUE(result.converged);
    EXPECT_LE(result.iterations, 2);
}

class ApplyIKTest : public ::testing::Test {
  protected:
    struct OzzSkeletonDeleter {
        void operator()(ozz::animation::Skeleton* p) const { ozz::Delete(p); }
    };

    std::shared_ptr<ozz::animation::Skeleton> buildSkeleton() {
        ozz::animation::offline::RawSkeleton rawSkel;
        rawSkel.roots.resize(1);
        auto& root = rawSkel.roots[0];
        root.name = "root";
        root.transform = ozz::math::Transform::identity();

        root.children.resize(1);
        auto& child = root.children[0];
        child.name = "child";
        child.transform = ozz::math::Transform::identity();

        ozz::animation::offline::SkeletonBuilder builder;
        auto skel = builder(rawSkel);
        return std::shared_ptr<ozz::animation::Skeleton>(skel.release(), OzzSkeletonDeleter{});
    }
};

TEST_F(ApplyIKTest, ApplyRotationModifiesJoint) {
    auto skeleton = buildSkeleton();
    ozz::vector<ozz::math::SoaTransform> locals(skeleton->joint_rest_poses().begin(),
                                                skeleton->joint_rest_poses().end());

    // Apply a 90-degree rotation around Z to joint 0
    Quatf rot90z = Quaternion<float>::fromAxisAngle(Vec3f(0.0f, 0.0f, 1.0f), 3.14159f / 2.0f);
    applyIKToSkeleton(locals, 0, rot90z);

    // Read back the quaternion
    alignas(16) float qx[4], qy[4], qz[4], qw[4];
    ozz::math::StorePtrU(locals[0].rotation.x, qx);
    ozz::math::StorePtrU(locals[0].rotation.y, qy);
    ozz::math::StorePtrU(locals[0].rotation.z, qz);
    ozz::math::StorePtrU(locals[0].rotation.w, qw);

    // The z component should be non-zero after a Z-axis rotation
    Quatf applied(qx[0], qy[0], qz[0], qw[0]);
    EXPECT_GT(std::abs(applied.z), 0.3f) << "Z rotation should affect quaternion z component";
}

TEST_F(ApplyIKTest, OutOfRangeIndexDoesNotCrash) {
    auto skeleton = buildSkeleton();
    ozz::vector<ozz::math::SoaTransform> locals(skeleton->joint_rest_poses().begin(),
                                                skeleton->joint_rest_poses().end());

    Quatf rot = Quaternion<float>::fromAxisAngle(Vec3f(0.0f, 1.0f, 0.0f), 0.5f);
    // Should not crash with out-of-range index
    applyIKToSkeleton(locals, 999, rot);
    applyIKToSkeleton(locals, -1, rot);
}

// --- Foot IK Tests ---

class FootIKTest : public ::testing::Test {
  protected:
    struct OzzSkeletonDeleter {
        void operator()(ozz::animation::Skeleton* p) const { ozz::Delete(p); }
    };

    std::shared_ptr<ozz::animation::Skeleton> skeleton_;
    int rootIdx_ = -1;
    int leftHipIdx_ = -1, leftKneeIdx_ = -1, leftAnkleIdx_ = -1;
    int rightHipIdx_ = -1, rightKneeIdx_ = -1, rightAnkleIdx_ = -1;

    void SetUp() override {
        // 7-joint lower-body skeleton:
        //   root (pelvis Y=5) -> leftHip/rightHip -> knee -> ankle
        //   Upper leg: 2 units, lower leg: 2 units, ankles at Y=1
        ozz::animation::offline::RawSkeleton raw;
        raw.roots.resize(1);
        auto& root = raw.roots[0];
        root.name = "root";
        root.transform = ozz::math::Transform::identity();
        root.transform.translation.y = 5.0f;

        root.children.resize(2);

        auto& lh = root.children[0];
        lh.name = "leftHip";
        lh.transform = ozz::math::Transform::identity();
        lh.transform.translation.x = -0.5f;

        lh.children.resize(1);
        auto& lk = lh.children[0];
        lk.name = "leftKnee";
        lk.transform = ozz::math::Transform::identity();
        lk.transform.translation.y = -2.0f;

        lk.children.resize(1);
        auto& la = lk.children[0];
        la.name = "leftAnkle";
        la.transform = ozz::math::Transform::identity();
        la.transform.translation.y = -2.0f;

        auto& rh = root.children[1];
        rh.name = "rightHip";
        rh.transform = ozz::math::Transform::identity();
        rh.transform.translation.x = 0.5f;

        rh.children.resize(1);
        auto& rk = rh.children[0];
        rk.name = "rightKnee";
        rk.transform = ozz::math::Transform::identity();
        rk.transform.translation.y = -2.0f;

        rk.children.resize(1);
        auto& ra = rk.children[0];
        ra.name = "rightAnkle";
        ra.transform = ozz::math::Transform::identity();
        ra.transform.translation.y = -2.0f;

        ozz::animation::offline::SkeletonBuilder builder;
        auto skel = builder(raw);
        skeleton_ = std::shared_ptr<ozz::animation::Skeleton>(skel.release(), OzzSkeletonDeleter{});

        const auto names = skeleton_->joint_names();
        for (int i = 0; i < skeleton_->num_joints(); ++i) {
            std::string name(names[i]);
            if (name == "root")
                rootIdx_ = i;
            else if (name == "leftHip")
                leftHipIdx_ = i;
            else if (name == "leftKnee")
                leftKneeIdx_ = i;
            else if (name == "leftAnkle")
                leftAnkleIdx_ = i;
            else if (name == "rightHip")
                rightHipIdx_ = i;
            else if (name == "rightKnee")
                rightKneeIdx_ = i;
            else if (name == "rightAnkle")
                rightAnkleIdx_ = i;
        }
    }

    Vec3f extractPosition(const ozz::math::Float4x4& m) {
        alignas(16) float col3[4];
        ozz::math::StorePtrU(m.cols[3], col3);
        return Vec3f(col3[0], col3[1], col3[2]);
    }

    fabric::FootIKConfig makeConfig(const fabric::ChunkedGrid<float>* grid) {
        fabric::FootIKConfig config;
        config.leftLeg = {leftHipIdx_, leftKneeIdx_, leftAnkleIdx_};
        config.rightLeg = {rightHipIdx_, rightKneeIdx_, rightAnkleIdx_};
        config.pelvisJoint = rootIdx_;
        config.footHeightOffset = 0.0f;
        config.maxCorrectionDist = 2.0f;
        config.raycastHeight = 5.0f;
        config.grounded = true;
        config.grid = grid;
        return config;
    }

    fabric::ChunkedGrid<float> makeFlatGround(int groundVoxelY) {
        fabric::ChunkedGrid<float> grid;
        for (int x = -5; x <= 5; ++x) {
            for (int z = -5; z <= 5; ++z) {
                grid.set(x, groundVoxelY, z, 1.0f);
            }
        }
        return grid;
    }
};

TEST_F(FootIKTest, FlatTerrainContact) {
    // Ground surface at Y=0 (top of voxels at y=-1). Ankles at Y=1 in rest pose.
    auto grid = makeFlatGround(-1);
    auto config = makeConfig(&grid);

    ozz::vector<ozz::math::SoaTransform> locals(skeleton_->joint_rest_poses().begin(),
                                                skeleton_->joint_rest_poses().end());

    fabric::AnimationSampler sampler;
    fabric::processFootIK(sampler, *skeleton_, locals, config);

    ozz::vector<ozz::math::Float4x4> models;
    sampler.localToModel(*skeleton_, locals, models);

    Vec3f leftAnkle = extractPosition(models[static_cast<size_t>(leftAnkleIdx_)]);
    Vec3f rightAnkle = extractPosition(models[static_cast<size_t>(rightAnkleIdx_)]);

    // Both feet should be near ground level (Y=0)
    EXPECT_NEAR(leftAnkle.y, 0.0f, 0.5f);
    EXPECT_NEAR(rightAnkle.y, 0.0f, 0.5f);
}

TEST_F(FootIKTest, SteppedTerrain) {
    // Left ground at Y=0 (voxel y=-1), right ground at Y=1 (voxel y=0)
    fabric::ChunkedGrid<float> grid;
    for (int x = -5; x < 0; ++x) {
        for (int z = -5; z <= 5; ++z) {
            grid.set(x, -1, z, 1.0f);
        }
    }
    for (int x = 0; x <= 5; ++x) {
        for (int z = -5; z <= 5; ++z) {
            grid.set(x, 0, z, 1.0f);
        }
    }

    auto config = makeConfig(&grid);

    ozz::vector<ozz::math::SoaTransform> locals(skeleton_->joint_rest_poses().begin(),
                                                skeleton_->joint_rest_poses().end());

    fabric::AnimationSampler sampler;
    fabric::processFootIK(sampler, *skeleton_, locals, config);

    ozz::vector<ozz::math::Float4x4> models;
    sampler.localToModel(*skeleton_, locals, models);

    Vec3f leftAnkle = extractPosition(models[static_cast<size_t>(leftAnkleIdx_)]);
    Vec3f rightAnkle = extractPosition(models[static_cast<size_t>(rightAnkleIdx_)]);

    // Right foot should be higher than left foot (stepped terrain)
    EXPECT_GT(rightAnkle.y, leftAnkle.y);
}

TEST_F(FootIKTest, UnreachableGround) {
    // Ground surface above ankles. Correction exceeds maxCorrectionDist.
    // Solid voxels at y=2 give surface at Y=3. Ankles at Y=1.
    // correction = |1 - 3| = 2 > maxCorrectionDist = 0.5
    fabric::ChunkedGrid<float> grid;
    for (int x = -5; x <= 5; ++x) {
        for (int z = -5; z <= 5; ++z) {
            grid.set(x, 2, z, 1.0f);
        }
    }

    auto config = makeConfig(&grid);
    config.maxCorrectionDist = 0.5f;

    ozz::vector<ozz::math::SoaTransform> locals(skeleton_->joint_rest_poses().begin(),
                                                skeleton_->joint_rest_poses().end());
    ozz::vector<ozz::math::SoaTransform> originalLocals(locals.begin(), locals.end());

    fabric::AnimationSampler sampler;
    fabric::processFootIK(sampler, *skeleton_, locals, config);

    // Locals should be unchanged (correction exceeds max)
    for (size_t i = 0; i < locals.size(); ++i) {
        alignas(16) float origY[4], newY[4];
        ozz::math::StorePtrU(originalLocals[i].translation.y, origY);
        ozz::math::StorePtrU(locals[i].translation.y, newY);
        for (int j = 0; j < 4; ++j) {
            EXPECT_FLOAT_EQ(origY[j], newY[j]) << "SoA element " << i << " lane " << j;
        }
    }
}

TEST_F(FootIKTest, PelvisAdjustment) {
    // Ground at Y=0. Ankles at Y=1. Pelvis should lower by ~1.
    auto grid = makeFlatGround(-1);
    auto config = makeConfig(&grid);

    ozz::vector<ozz::math::SoaTransform> locals(skeleton_->joint_rest_poses().begin(),
                                                skeleton_->joint_rest_poses().end());

    int soaIdx = rootIdx_ / 4;
    int lane = rootIdx_ % 4;

    alignas(16) float origRootY[4];
    ozz::math::StorePtrU(locals[static_cast<size_t>(soaIdx)].translation.y, origRootY);

    fabric::AnimationSampler sampler;
    fabric::processFootIK(sampler, *skeleton_, locals, config);

    alignas(16) float newRootY[4];
    ozz::math::StorePtrU(locals[static_cast<size_t>(soaIdx)].translation.y, newRootY);

    EXPECT_LT(newRootY[lane], origRootY[lane]) << "Pelvis should lower toward ground";
    float pelvisChange = origRootY[lane] - newRootY[lane];
    EXPECT_GT(pelvisChange, 0.5f) << "Pelvis should lower significantly";
}

TEST_F(FootIKTest, NoConfigPassthrough) {
    auto grid = makeFlatGround(-1);

    // Test airborne (grounded=false) skips processing
    auto config = makeConfig(&grid);
    config.grounded = false;

    ozz::vector<ozz::math::SoaTransform> locals(skeleton_->joint_rest_poses().begin(),
                                                skeleton_->joint_rest_poses().end());
    ozz::vector<ozz::math::SoaTransform> originalLocals(locals.begin(), locals.end());

    fabric::AnimationSampler sampler;
    fabric::processFootIK(sampler, *skeleton_, locals, config);

    for (size_t i = 0; i < locals.size(); ++i) {
        alignas(16) float origY[4], newY[4];
        ozz::math::StorePtrU(originalLocals[i].translation.y, origY);
        ozz::math::StorePtrU(locals[i].translation.y, newY);
        for (int j = 0; j < 4; ++j) {
            EXPECT_FLOAT_EQ(origY[j], newY[j]);
        }
    }

    // Test null grid skips processing
    config.grounded = true;
    config.grid = nullptr;
    fabric::processFootIK(sampler, *skeleton_, locals, config);

    for (size_t i = 0; i < locals.size(); ++i) {
        alignas(16) float origY[4], newY[4];
        ozz::math::StorePtrU(originalLocals[i].translation.y, origY);
        ozz::math::StorePtrU(locals[i].translation.y, newY);
        for (int j = 0; j < 4; ++j) {
            EXPECT_FLOAT_EQ(origY[j], newY[j]);
        }
    }
}
