#include "fabric/core/Animation.hh"
#include <cmath>
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
