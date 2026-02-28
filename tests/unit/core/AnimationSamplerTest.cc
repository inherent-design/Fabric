#include "fabric/core/Animation.hh"
#include <cmath>
#include <cstring>
#include <gtest/gtest.h>
#include <ozz/animation/offline/animation_builder.h>
#include <ozz/animation/offline/raw_animation.h>
#include <ozz/animation/offline/raw_skeleton.h>
#include <ozz/animation/offline/skeleton_builder.h>
#include <ozz/base/maths/quaternion.h>
#include <ozz/base/maths/vec_float.h>
#include <ozz/base/memory/allocator.h>

using namespace fabric;

namespace {

// ozz allocates via its own aligned allocator; must use ozz::Delete for cleanup
struct OzzSkeletonDeleter {
    void operator()(ozz::animation::Skeleton* p) const { ozz::Delete(p); }
};

struct OzzAnimationDeleter {
    void operator()(ozz::animation::Animation* p) const { ozz::Delete(p); }
};

// Build a simple 3-joint skeleton: root -> child -> tip
// All joints at rest pose identity, spaced 1 unit apart on Y axis.
std::shared_ptr<ozz::animation::Skeleton> buildTestSkeleton() {
    ozz::animation::offline::RawSkeleton rawSkel;
    rawSkel.roots.resize(1);
    auto& root = rawSkel.roots[0];
    root.name = "root";
    root.transform = ozz::math::Transform::identity();

    root.children.resize(1);
    auto& child = root.children[0];
    child.name = "child";
    child.transform = ozz::math::Transform::identity();
    child.transform.translation = ozz::math::Float3(0.0f, 1.0f, 0.0f);

    child.children.resize(1);
    auto& tip = child.children[0];
    tip.name = "tip";
    tip.transform = ozz::math::Transform::identity();
    tip.transform.translation = ozz::math::Float3(0.0f, 1.0f, 0.0f);

    ozz::animation::offline::SkeletonBuilder skelBuilder;
    auto skeleton = skelBuilder(rawSkel);
    return std::shared_ptr<ozz::animation::Skeleton>(skeleton.release(), OzzSkeletonDeleter{});
}

// Build a simple animation that translates the root joint from (0,0,0) to (10,0,0) over 1 second.
std::shared_ptr<ozz::animation::Animation> buildTestAnimation(int numJoints) {
    ozz::animation::offline::RawAnimation rawAnim;
    rawAnim.duration = 1.0f;
    rawAnim.tracks.resize(static_cast<size_t>(numJoints));

    // Root track: translate from origin to (10,0,0)
    auto& rootTrack = rawAnim.tracks[0];
    rootTrack.translations.push_back({0.0f, ozz::math::Float3(0.0f, 0.0f, 0.0f)});
    rootTrack.translations.push_back({1.0f, ozz::math::Float3(10.0f, 0.0f, 0.0f)});
    rootTrack.rotations.push_back({0.0f, ozz::math::Quaternion::identity()});
    rootTrack.scales.push_back({0.0f, ozz::math::Float3(1.0f, 1.0f, 1.0f)});

    // Other tracks: identity
    for (int i = 1; i < numJoints; ++i) {
        auto& track = rawAnim.tracks[static_cast<size_t>(i)];
        track.translations.push_back({0.0f, ozz::math::Float3(0.0f, 0.0f, 0.0f)});
        track.rotations.push_back({0.0f, ozz::math::Quaternion::identity()});
        track.scales.push_back({0.0f, ozz::math::Float3(1.0f, 1.0f, 1.0f)});
    }

    // Override child track: keep its local translation (0,1,0)
    rawAnim.tracks[1].translations.clear();
    rawAnim.tracks[1].translations.push_back({0.0f, ozz::math::Float3(0.0f, 1.0f, 0.0f)});

    // Override tip track: keep local translation (0,1,0)
    rawAnim.tracks[2].translations.clear();
    rawAnim.tracks[2].translations.push_back({0.0f, ozz::math::Float3(0.0f, 1.0f, 0.0f)});

    ozz::animation::offline::AnimationBuilder animBuilder;
    auto anim = animBuilder(rawAnim);
    return std::shared_ptr<ozz::animation::Animation>(anim.release(), OzzAnimationDeleter{});
}

} // namespace

class AnimationSamplerTest : public ::testing::Test {
  protected:
    std::shared_ptr<ozz::animation::Skeleton> skeleton;
    std::shared_ptr<ozz::animation::Animation> animation;

    void SetUp() override {
        skeleton = buildTestSkeleton();
        animation = buildTestAnimation(skeleton->num_joints());
    }
};

TEST_F(AnimationSamplerTest, SampleAtStartProducesStartPose) {
    AnimationSampler sampler;
    ozz::vector<ozz::math::SoaTransform> locals;
    sampler.sample(*animation, *skeleton, 0.0f, locals);

    EXPECT_EQ(locals.size(), static_cast<size_t>(skeleton->num_soa_joints()));
}

TEST_F(AnimationSamplerTest, SampleOutputMatchesJointCount) {
    AnimationSampler sampler;
    ozz::vector<ozz::math::SoaTransform> locals;
    sampler.sample(*animation, *skeleton, 0.5f, locals);

    // SoA packs 4 joints per element; 3 joints requires ceil(3/4) = 1 SoA element
    EXPECT_GE(locals.size() * 4, static_cast<size_t>(skeleton->num_joints()));
}

TEST_F(AnimationSamplerTest, LocalToModelProducesCorrectJointCount) {
    AnimationSampler sampler;
    ozz::vector<ozz::math::SoaTransform> locals;
    sampler.sample(*animation, *skeleton, 0.0f, locals);

    ozz::vector<ozz::math::Float4x4> models;
    sampler.localToModel(*skeleton, locals, models);

    EXPECT_EQ(models.size(), static_cast<size_t>(skeleton->num_joints()));
}

TEST_F(AnimationSamplerTest, ModelSpaceRootTranslationAtMidpoint) {
    AnimationSampler sampler;
    ozz::vector<ozz::math::SoaTransform> locals;
    sampler.sample(*animation, *skeleton, 0.5f, locals);

    ozz::vector<ozz::math::Float4x4> models;
    sampler.localToModel(*skeleton, locals, models);

    // Root joint at midpoint should be near (5,0,0) in model space
    Matrix4x4<float> rootModel = ozzToMatrix4x4(models[0]);
    EXPECT_NEAR(rootModel(0, 3), 5.0f, 0.5f);
}

TEST_F(AnimationSamplerTest, BlendTwoPosesHalfweight) {
    AnimationSampler sampler;
    ozz::vector<ozz::math::SoaTransform> localsA;
    ozz::vector<ozz::math::SoaTransform> localsB;

    sampler.sample(*animation, *skeleton, 0.0f, localsA);
    sampler.sample(*animation, *skeleton, 1.0f, localsB);

    ozz::vector<ozz::math::SoaTransform> blended;
    sampler.blend(*skeleton, localsA, localsB, 0.5f, blended);

    EXPECT_EQ(blended.size(), static_cast<size_t>(skeleton->num_soa_joints()));

    // Convert blended to model space to verify interpolation
    ozz::vector<ozz::math::Float4x4> models;
    sampler.localToModel(*skeleton, blended, models);
    Matrix4x4<float> rootModel = ozzToMatrix4x4(models[0]);

    // Blend at 0.5 between (0,0,0) and (10,0,0) should be near (5,0,0)
    EXPECT_NEAR(rootModel(0, 3), 5.0f, 0.5f);
}

TEST_F(AnimationSamplerTest, BlendWeightZeroReturnsFirstPose) {
    AnimationSampler sampler;
    ozz::vector<ozz::math::SoaTransform> localsA;
    ozz::vector<ozz::math::SoaTransform> localsB;

    sampler.sample(*animation, *skeleton, 0.0f, localsA);
    sampler.sample(*animation, *skeleton, 1.0f, localsB);

    ozz::vector<ozz::math::SoaTransform> blended;
    sampler.blend(*skeleton, localsA, localsB, 0.0f, blended);

    ozz::vector<ozz::math::Float4x4> models;
    sampler.localToModel(*skeleton, blended, models);
    Matrix4x4<float> rootModel = ozzToMatrix4x4(models[0]);

    // Weight 0 means 100% pose A (start at 0,0,0)
    EXPECT_NEAR(rootModel(0, 3), 0.0f, 0.5f);
}

TEST_F(AnimationSamplerTest, BlendWeightOneReturnsSecondPose) {
    AnimationSampler sampler;
    ozz::vector<ozz::math::SoaTransform> localsA;
    ozz::vector<ozz::math::SoaTransform> localsB;

    sampler.sample(*animation, *skeleton, 0.0f, localsA);
    sampler.sample(*animation, *skeleton, 1.0f, localsB);

    ozz::vector<ozz::math::SoaTransform> blended;
    sampler.blend(*skeleton, localsA, localsB, 1.0f, blended);

    ozz::vector<ozz::math::Float4x4> models;
    sampler.localToModel(*skeleton, blended, models);
    Matrix4x4<float> rootModel = ozzToMatrix4x4(models[0]);

    // Weight 1 means 100% pose B (end at 10,0,0)
    EXPECT_NEAR(rootModel(0, 3), 10.0f, 0.5f);
}

TEST_F(AnimationSamplerTest, ComputeSkinningMatricesReturnsCorrectCount) {
    AnimationSampler sampler;
    ozz::vector<ozz::math::SoaTransform> locals;
    sampler.sample(*animation, *skeleton, 0.0f, locals);

    ozz::vector<ozz::math::Float4x4> models;
    sampler.localToModel(*skeleton, locals, models);

    auto skinning = sampler.computeSkinningMatrices(*skeleton, models);
    EXPECT_EQ(skinning.size(), static_cast<size_t>(skeleton->num_joints()));
}

TEST_F(AnimationSamplerTest, SkinningMatricesAtRestPoseAreIdentity) {
    // At rest pose, skinning = model * inverse(restModel) = identity
    AnimationSampler sampler;

    // Sample rest pose by using the skeleton's rest pose directly
    ozz::vector<ozz::math::SoaTransform> locals(skeleton->joint_rest_poses().begin(),
                                                skeleton->joint_rest_poses().end());

    ozz::vector<ozz::math::Float4x4> models;
    sampler.localToModel(*skeleton, locals, models);

    auto skinning = sampler.computeSkinningMatrices(*skeleton, models);

    for (size_t i = 0; i < skinning.size(); ++i) {
        // Each skinning matrix should be near identity
        EXPECT_NEAR(skinning[i](0, 0), 1.0f, 1e-4f) << "joint " << i;
        EXPECT_NEAR(skinning[i](1, 1), 1.0f, 1e-4f) << "joint " << i;
        EXPECT_NEAR(skinning[i](2, 2), 1.0f, 1e-4f) << "joint " << i;
        EXPECT_NEAR(skinning[i](3, 3), 1.0f, 1e-4f) << "joint " << i;
        EXPECT_NEAR(skinning[i](0, 3), 0.0f, 1e-4f) << "joint " << i;
        EXPECT_NEAR(skinning[i](1, 3), 0.0f, 1e-4f) << "joint " << i;
        EXPECT_NEAR(skinning[i](2, 3), 0.0f, 1e-4f) << "joint " << i;
    }
}

TEST_F(AnimationSamplerTest, SampleAtEndProducesEndPose) {
    AnimationSampler sampler;
    ozz::vector<ozz::math::SoaTransform> locals;
    sampler.sample(*animation, *skeleton, 1.0f, locals);

    ozz::vector<ozz::math::Float4x4> models;
    sampler.localToModel(*skeleton, locals, models);

    Matrix4x4<float> rootModel = ozzToMatrix4x4(models[0]);
    EXPECT_NEAR(rootModel(0, 3), 10.0f, 0.5f);
}

TEST_F(AnimationSamplerTest, RegisterAnimationSystemDoesNotCrash) {
    flecs::world world;
    registerAnimationSystem(world);
    // Just verify registration completes without error
    world.progress(0.016f);
}

// --- Layered blending tests ---

namespace {

// Build a 5-joint humanoid skeleton for layered blend tests:
// hips -> [left_leg, right_leg, spine -> head]
std::shared_ptr<ozz::animation::Skeleton> buildHumanoidTestSkeleton() {
    ozz::animation::offline::RawSkeleton rawSkel;
    rawSkel.roots.resize(1);
    auto& root = rawSkel.roots[0];
    root.name = "hips";
    root.transform = ozz::math::Transform::identity();

    root.children.resize(3);

    auto& leftLeg = root.children[0];
    leftLeg.name = "left_leg";
    leftLeg.transform = ozz::math::Transform::identity();
    leftLeg.transform.translation = ozz::math::Float3(-0.5f, -1.0f, 0.0f);

    auto& rightLeg = root.children[1];
    rightLeg.name = "right_leg";
    rightLeg.transform = ozz::math::Transform::identity();
    rightLeg.transform.translation = ozz::math::Float3(0.5f, -1.0f, 0.0f);

    auto& spine = root.children[2];
    spine.name = "spine";
    spine.transform = ozz::math::Transform::identity();
    spine.transform.translation = ozz::math::Float3(0.0f, 1.0f, 0.0f);

    spine.children.resize(1);
    auto& head = spine.children[0];
    head.name = "head";
    head.transform = ozz::math::Transform::identity();
    head.transform.translation = ozz::math::Float3(0.0f, 0.5f, 0.0f);

    ozz::animation::offline::SkeletonBuilder skelBuilder;
    auto skeleton = skelBuilder(rawSkel);
    return std::shared_ptr<ozz::animation::Skeleton>(skeleton.release(), OzzSkeletonDeleter{});
}

// Build an animation for the humanoid skeleton where the root translates to endPos.
// Non-root tracks use identity (0,0,0) local position.
std::shared_ptr<ozz::animation::Animation> buildHumanoidAnimation(int numJoints, ozz::math::Float3 endPos) {
    ozz::animation::offline::RawAnimation rawAnim;
    rawAnim.duration = 1.0f;
    rawAnim.tracks.resize(static_cast<size_t>(numJoints));

    auto& rootTrack = rawAnim.tracks[0];
    rootTrack.translations.push_back({0.0f, ozz::math::Float3(0.0f, 0.0f, 0.0f)});
    rootTrack.translations.push_back({1.0f, endPos});
    rootTrack.rotations.push_back({0.0f, ozz::math::Quaternion::identity()});
    rootTrack.scales.push_back({0.0f, ozz::math::Float3(1.0f, 1.0f, 1.0f)});

    for (int i = 1; i < numJoints; ++i) {
        auto& track = rawAnim.tracks[static_cast<size_t>(i)];
        track.translations.push_back({0.0f, ozz::math::Float3(0.0f, 0.0f, 0.0f)});
        track.rotations.push_back({0.0f, ozz::math::Quaternion::identity()});
        track.scales.push_back({0.0f, ozz::math::Float3(1.0f, 1.0f, 1.0f)});
    }

    ozz::animation::offline::AnimationBuilder animBuilder;
    auto anim = animBuilder(rawAnim);
    return std::shared_ptr<ozz::animation::Animation>(anim.release(), OzzAnimationDeleter{});
}

int findJointByName(const ozz::animation::Skeleton& skel, const char* name) {
    const auto names = skel.joint_names();
    for (int i = 0; i < skel.num_joints(); ++i) {
        if (std::strcmp(names[i], name) == 0) {
            return i;
        }
    }
    return -1;
}

} // namespace

class AnimationLayeredTest : public ::testing::Test {
  protected:
    std::shared_ptr<ozz::animation::Skeleton> skeleton;
    std::shared_ptr<ozz::animation::Animation> animA;
    std::shared_ptr<ozz::animation::Animation> animB;

    void SetUp() override {
        skeleton = buildHumanoidTestSkeleton();
        animA = buildHumanoidAnimation(skeleton->num_joints(), ozz::math::Float3(10.0f, 0.0f, 0.0f));
        animB = buildHumanoidAnimation(skeleton->num_joints(), ozz::math::Float3(0.0f, 0.0f, 10.0f));
    }
};

TEST_F(AnimationLayeredTest, UpperBodyMaskPartitions) {
    auto mask = JointMask::createUpperBody(*skeleton);
    EXPECT_EQ(mask.weights.size(), static_cast<size_t>(skeleton->num_soa_joints()));

    // Extract per-joint weights from SoA
    const int numJoints = skeleton->num_joints();
    std::vector<float> perJoint(static_cast<size_t>(numJoints));
    for (int i = 0; i < numJoints; ++i) {
        alignas(16) float lane[4];
        ozz::math::StorePtrU(mask.weights[static_cast<size_t>(i / 4)], lane);
        perJoint[static_cast<size_t>(i)] = lane[i % 4];
    }

    // Lower body joints should be 0
    int hips = findJointByName(*skeleton, "hips");
    int leftLeg = findJointByName(*skeleton, "left_leg");
    int rightLeg = findJointByName(*skeleton, "right_leg");
    ASSERT_GE(hips, 0);
    ASSERT_GE(leftLeg, 0);
    ASSERT_GE(rightLeg, 0);
    EXPECT_FLOAT_EQ(perJoint[static_cast<size_t>(hips)], 0.0f);
    EXPECT_FLOAT_EQ(perJoint[static_cast<size_t>(leftLeg)], 0.0f);
    EXPECT_FLOAT_EQ(perJoint[static_cast<size_t>(rightLeg)], 0.0f);

    // Upper body joints should be 1
    int spineIdx = findJointByName(*skeleton, "spine");
    int head = findJointByName(*skeleton, "head");
    ASSERT_GE(spineIdx, 0);
    ASSERT_GE(head, 0);
    EXPECT_FLOAT_EQ(perJoint[static_cast<size_t>(spineIdx)], 1.0f);
    EXPECT_FLOAT_EQ(perJoint[static_cast<size_t>(head)], 1.0f);
}

TEST_F(AnimationLayeredTest, FullBodyFallback) {
    AnimationSampler sampler;
    ozz::vector<ozz::math::SoaTransform> localsA, localsB;
    sampler.sampleLayer(0, *animA, *skeleton, 1.0f, localsA);
    sampler.sampleLayer(1, *animB, *skeleton, 1.0f, localsB);

    // Blend with existing method
    ozz::vector<ozz::math::SoaTransform> expectedBlend;
    sampler.blend(*skeleton, localsA, localsB, 0.5f, expectedBlend);

    // Blend with blendLayered (no masks, equal weights)
    ozz::animation::BlendingJob::Layer layers[2];
    layers[0].weight = 0.5f;
    layers[0].transform = ozz::make_span(localsA);
    layers[1].weight = 0.5f;
    layers[1].transform = ozz::make_span(localsB);

    ozz::vector<ozz::math::SoaTransform> layeredBlend;
    sampler.blendLayered(*skeleton, ozz::span<const ozz::animation::BlendingJob::Layer>(layers, 2), layeredBlend);

    // Both should produce the same model-space result
    ozz::vector<ozz::math::Float4x4> expectedModels, layeredModels;
    sampler.localToModel(*skeleton, expectedBlend, expectedModels);
    sampler.localToModel(*skeleton, layeredBlend, layeredModels);

    for (int i = 0; i < skeleton->num_joints(); ++i) {
        auto expected = ozzToMatrix4x4(expectedModels[static_cast<size_t>(i)]);
        auto layered = ozzToMatrix4x4(layeredModels[static_cast<size_t>(i)]);
        for (int j = 0; j < 16; ++j) {
            EXPECT_NEAR(expected.elements[static_cast<size_t>(j)], layered.elements[static_cast<size_t>(j)], 1e-4f)
                << "joint " << i << " element " << j;
        }
    }
}

TEST_F(AnimationLayeredTest, WeightZeroLayerIgnored) {
    AnimationSampler sampler;
    ozz::vector<ozz::math::SoaTransform> localsA, localsB;
    sampler.sampleLayer(0, *animA, *skeleton, 1.0f, localsA);
    sampler.sampleLayer(1, *animB, *skeleton, 1.0f, localsB);

    // Layer 0 at weight 1, layer 1 at weight 0
    ozz::animation::BlendingJob::Layer layers[2];
    layers[0].weight = 1.0f;
    layers[0].transform = ozz::make_span(localsA);
    layers[1].weight = 0.0f;
    layers[1].transform = ozz::make_span(localsB);

    ozz::vector<ozz::math::SoaTransform> blended;
    sampler.blendLayered(*skeleton, ozz::span<const ozz::animation::BlendingJob::Layer>(layers, 2), blended);

    // Result should match layer 0 only (animA at t=1: root at 10,0,0)
    ozz::vector<ozz::math::Float4x4> models;
    sampler.localToModel(*skeleton, blended, models);

    int hipsIdx = findJointByName(*skeleton, "hips");
    ASSERT_GE(hipsIdx, 0);
    auto rootModel = ozzToMatrix4x4(models[static_cast<size_t>(hipsIdx)]);
    EXPECT_NEAR(rootModel(0, 3), 10.0f, 0.5f);
    EXPECT_NEAR(rootModel(2, 3), 0.0f, 0.5f);
}

TEST_F(AnimationLayeredTest, ThreeLayerBlend) {
    AnimationSampler sampler;
    auto animC = buildHumanoidAnimation(skeleton->num_joints(), ozz::math::Float3(0.0f, 10.0f, 0.0f));

    ozz::vector<ozz::math::SoaTransform> localsA, localsB, localsC;
    sampler.sampleLayer(0, *animA, *skeleton, 1.0f, localsA);
    sampler.sampleLayer(1, *animB, *skeleton, 1.0f, localsB);
    sampler.sampleLayer(2, *animC, *skeleton, 1.0f, localsC);

    ozz::animation::BlendingJob::Layer layers[3];
    layers[0].weight = 1.0f;
    layers[0].transform = ozz::make_span(localsA);
    layers[1].weight = 1.0f;
    layers[1].transform = ozz::make_span(localsB);
    layers[2].weight = 1.0f;
    layers[2].transform = ozz::make_span(localsC);

    ozz::vector<ozz::math::SoaTransform> blended;
    sampler.blendLayered(*skeleton, ozz::span<const ozz::animation::BlendingJob::Layer>(layers, 3), blended);

    // Equal weight blend of (10,0,0), (0,0,10), (0,10,0) should average
    ozz::vector<ozz::math::Float4x4> models;
    sampler.localToModel(*skeleton, blended, models);

    int hipsIdx = findJointByName(*skeleton, "hips");
    ASSERT_GE(hipsIdx, 0);
    auto rootModel = ozzToMatrix4x4(models[static_cast<size_t>(hipsIdx)]);
    EXPECT_NEAR(rootModel(0, 3), 10.0f / 3.0f, 0.5f);
    EXPECT_NEAR(rootModel(1, 3), 10.0f / 3.0f, 0.5f);
    EXPECT_NEAR(rootModel(2, 3), 10.0f / 3.0f, 0.5f);
}

TEST_F(AnimationLayeredTest, SoaAlignmentCorrect) {
    auto mask = JointMask::createUpperBody(*skeleton);

    // SoA elements should equal num_soa_joints
    EXPECT_EQ(mask.weights.size(), static_cast<size_t>(skeleton->num_soa_joints()));

    // num_soa_joints should be ceil(num_joints / 4)
    const int expectedSoa = (skeleton->num_joints() + 3) / 4;
    EXPECT_EQ(skeleton->num_soa_joints(), expectedSoa);

    // Full body mask should also be properly sized
    auto fullMask = JointMask::createFullBody(*skeleton);
    EXPECT_EQ(fullMask.weights.size(), static_cast<size_t>(skeleton->num_soa_joints()));
}

TEST_F(AnimationLayeredTest, PartialBlendWithMask) {
    AnimationSampler sampler;
    auto mask = JointMask::createUpperBody(*skeleton);

    // Pose A: animA at t=0 (root at origin)
    // Pose B: animA at t=1 (root at 10,0,0)
    ozz::vector<ozz::math::SoaTransform> localsA, localsB;
    sampler.sampleLayer(0, *animA, *skeleton, 0.0f, localsA);
    sampler.sampleLayer(1, *animA, *skeleton, 1.0f, localsB);

    // Layer 0: full body, pose A (root at 0,0,0)
    // Layer 1: upper body only, pose B (root at 10,0,0)
    ozz::animation::BlendingJob::Layer layers[2];
    layers[0].weight = 1.0f;
    layers[0].transform = ozz::make_span(localsA);
    layers[1].weight = 1.0f;
    layers[1].transform = ozz::make_span(localsB);
    layers[1].joint_weights = ozz::make_span(mask.weights);

    ozz::vector<ozz::math::SoaTransform> blended;
    sampler.blendLayered(*skeleton, ozz::span<const ozz::animation::BlendingJob::Layer>(layers, 2), blended);

    // Root (hips, lower body, mask=0 on layer 1): only layer 0 contributes
    // So root should be at (0,0,0) from pose A
    ozz::vector<ozz::math::Float4x4> models;
    sampler.localToModel(*skeleton, blended, models);

    int hipsIdx = findJointByName(*skeleton, "hips");
    ASSERT_GE(hipsIdx, 0);
    auto rootModel = ozzToMatrix4x4(models[static_cast<size_t>(hipsIdx)]);
    EXPECT_NEAR(rootModel(0, 3), 0.0f, 0.5f);
}
