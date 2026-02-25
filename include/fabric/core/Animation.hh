#pragma once

#include "fabric/core/Spatial.hh"
#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <flecs.h>
#include <ozz/animation/runtime/animation.h>
#include <ozz/animation/runtime/blending_job.h>
#include <ozz/animation/runtime/local_to_model_job.h>
#include <ozz/animation/runtime/sampling_job.h>
#include <ozz/animation/runtime/skeleton.h>
#include <ozz/base/containers/vector.h>
#include <ozz/base/maths/simd_math.h>
#include <ozz/base/maths/soa_transform.h>

namespace fabric {

// Maximum joints supported for humanoid characters (60-100 bones)
inline constexpr int kMaxJoints = 128;

// ECS component: shared skeleton reference
struct Skeleton {
    std::shared_ptr<ozz::animation::Skeleton> skeleton;
};

// ECS component: single animation clip reference
struct AnimationClip {
    std::shared_ptr<ozz::animation::Animation> animation;
    std::string name;
};

// ECS component: current playback state for an animation
struct AnimationState {
    std::shared_ptr<ozz::animation::Animation> clip;
    float time = 0.0f;
    float speed = 1.0f;
    bool loop = true;
    bool playing = true;
};

// ECS component: blend tree entry for layered/additive animation blending
struct AnimationBlendEntry {
    AnimationState state;
    float weight = 1.0f;
};

struct AnimationBlendTree {
    std::vector<AnimationBlendEntry> layers;
};

// ECS component: final skinning matrices for GPU submission
struct SkinningData {
    std::vector<std::array<float, 16>> jointMatrices;
};

// Samples, blends, and converts ozz animation data.
// Uses ozz SoA layout internally; all ozz::vector allocations are SIMD-aligned.
class AnimationSampler {
  public:
    // Sample an animation clip at a given time ratio [0,1], writing local-space
    // SoA transforms into locals. The context is managed internally per skeleton
    // track count.
    void sample(const ozz::animation::Animation& clip, const ozz::animation::Skeleton& skeleton, float time,
                ozz::vector<ozz::math::SoaTransform>& locals);

    // Blend two local-space pose buffers (a, b) by weight [0,1] into output.
    // Uses the skeleton rest pose for normalization when weights are low.
    void blend(const ozz::animation::Skeleton& skeleton, const ozz::vector<ozz::math::SoaTransform>& a,
               const ozz::vector<ozz::math::SoaTransform>& b, float weight,
               ozz::vector<ozz::math::SoaTransform>& output);

    // Convert local-space SoA transforms to model-space Float4x4 matrices
    // using the skeleton hierarchy.
    void localToModel(const ozz::animation::Skeleton& skeleton, const ozz::vector<ozz::math::SoaTransform>& locals,
                      ozz::vector<ozz::math::Float4x4>& models);

    // Compute final skinning matrices (model * inverse bind) and convert
    // to Fabric Matrix4x4 types suitable for GPU uniform upload.
    std::vector<Matrix4x4<float>> computeSkinningMatrices(const ozz::animation::Skeleton& skeleton,
                                                          const ozz::vector<ozz::math::Float4x4>& models);

  private:
    ozz::animation::SamplingJob::Context context_;
};

// Flecs system that queries entities with (Skeleton, AnimationState, SkinningData)
// and samples animation each frame.
void registerAnimationSystem(flecs::world& world);

// Type conversion utilities between ozz and Fabric math types.
// ozz uses SoA SIMD layout; these convert to/from Fabric column-major Matrix4x4.

// Convert ozz Float4x4 to Fabric column-major float[16]
void ozzToFabricMatrix(const ozz::math::Float4x4& src, float* dst);

// Convert Fabric column-major float[16] to ozz Float4x4
void fabricToOzzMatrix(const float* src, ozz::math::Float4x4& dst);

// Convert ozz Float4x4 to Fabric Matrix4x4<float>
Matrix4x4<float> ozzToMatrix4x4(const ozz::math::Float4x4& src);

// Convert Fabric Matrix4x4<float> to ozz Float4x4
ozz::math::Float4x4 matrix4x4ToOzz(const Matrix4x4<float>& src);

} // namespace fabric
