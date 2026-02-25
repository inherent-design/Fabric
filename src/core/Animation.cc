#include "fabric/core/Animation.hh"

#include "fabric/core/Log.hh"
#include "fabric/utils/ErrorHandling.hh"
#include "fabric/utils/Profiler.hh"
#include <ozz/base/maths/simd_math.h>

namespace fabric {

void ozzToFabricMatrix(const ozz::math::Float4x4& src, float* dst) {
    // ozz Float4x4 stores 4 columns as SimdFloat4 (128-bit SIMD).
    // Extract each column to 4 floats, writing column-major.
    for (int col = 0; col < 4; ++col) {
        alignas(16) float colData[4];
        ozz::math::StorePtrU(src.cols[col], colData);
        dst[col * 4 + 0] = colData[0];
        dst[col * 4 + 1] = colData[1];
        dst[col * 4 + 2] = colData[2];
        dst[col * 4 + 3] = colData[3];
    }
}

void fabricToOzzMatrix(const float* src, ozz::math::Float4x4& dst) {
    for (int col = 0; col < 4; ++col) {
        dst.cols[col] =
            ozz::math::simd_float4::Load(src[col * 4 + 0], src[col * 4 + 1], src[col * 4 + 2], src[col * 4 + 3]);
    }
}

Matrix4x4<float> ozzToMatrix4x4(const ozz::math::Float4x4& src) {
    Matrix4x4<float> result;
    ozzToFabricMatrix(src, result.elements.data());
    return result;
}

ozz::math::Float4x4 matrix4x4ToOzz(const Matrix4x4<float>& src) {
    ozz::math::Float4x4 result;
    fabricToOzzMatrix(src.elements.data(), result);
    return result;
}

// --- AnimationSampler ---

void AnimationSampler::sample(const ozz::animation::Animation& clip, const ozz::animation::Skeleton& skeleton,
                              float time, ozz::vector<ozz::math::SoaTransform>& locals) {
    FABRIC_ZONE_SCOPED;

    const int numSoaJoints = skeleton.num_soa_joints();
    locals.resize(static_cast<size_t>(numSoaJoints));

    // Resize context if needed (track count = num_joints, SoA aligned internally)
    if (context_.max_tracks() < skeleton.num_joints()) {
        context_.Resize(skeleton.num_joints());
    }

    ozz::animation::SamplingJob samplingJob;
    samplingJob.animation = &clip;
    samplingJob.context = &context_;
    samplingJob.ratio = time;
    samplingJob.output = ozz::make_span(locals);

    if (!samplingJob.Run()) {
        throwError("AnimationSampler::sample: SamplingJob failed");
    }
}

void AnimationSampler::blend(const ozz::animation::Skeleton& skeleton, const ozz::vector<ozz::math::SoaTransform>& a,
                             const ozz::vector<ozz::math::SoaTransform>& b, float weight,
                             ozz::vector<ozz::math::SoaTransform>& output) {
    FABRIC_ZONE_SCOPED;

    const int numSoaJoints = skeleton.num_soa_joints();
    output.resize(static_cast<size_t>(numSoaJoints));

    // Set up two layers: a at (1-weight), b at weight
    ozz::animation::BlendingJob::Layer layers[2];
    layers[0].weight = 1.0f - weight;
    layers[0].transform = ozz::make_span(a);
    layers[1].weight = weight;
    layers[1].transform = ozz::make_span(b);

    ozz::animation::BlendingJob blendJob;
    blendJob.layers = ozz::span<const ozz::animation::BlendingJob::Layer>(layers, 2);
    blendJob.rest_pose = skeleton.joint_rest_poses();
    blendJob.output = ozz::make_span(output);
    blendJob.threshold = 0.1f;

    if (!blendJob.Run()) {
        throwError("AnimationSampler::blend: BlendingJob failed");
    }
}

void AnimationSampler::localToModel(const ozz::animation::Skeleton& skeleton,
                                    const ozz::vector<ozz::math::SoaTransform>& locals,
                                    ozz::vector<ozz::math::Float4x4>& models) {
    FABRIC_ZONE_SCOPED;

    models.resize(static_cast<size_t>(skeleton.num_joints()));

    ozz::animation::LocalToModelJob ltmJob;
    ltmJob.skeleton = &skeleton;
    ltmJob.input = ozz::make_span(locals);
    ltmJob.output = ozz::make_span(models);

    if (!ltmJob.Run()) {
        throwError("AnimationSampler::localToModel: LocalToModelJob failed");
    }
}

std::vector<Matrix4x4<float>>
AnimationSampler::computeSkinningMatrices(const ozz::animation::Skeleton& skeleton,
                                          const ozz::vector<ozz::math::Float4x4>& models) {
    FABRIC_ZONE_SCOPED;

    const int numJoints = skeleton.num_joints();
    const auto& restPoses = skeleton.joint_rest_poses();

    // Compute inverse bind matrices from the rest pose using LocalToModelJob.
    // The inverse bind matrix for each joint is the inverse of its model-space
    // rest pose transform.
    ozz::vector<ozz::math::SoaTransform> restLocals(restPoses.begin(), restPoses.end());
    ozz::vector<ozz::math::Float4x4> restModels;
    restModels.resize(static_cast<size_t>(numJoints));

    ozz::animation::LocalToModelJob restLtm;
    restLtm.skeleton = &skeleton;
    restLtm.input = ozz::make_span(restLocals);
    restLtm.output = ozz::make_span(restModels);
    if (!restLtm.Run()) {
        throwError("AnimationSampler::computeSkinningMatrices: rest pose LocalToModelJob failed");
    }

    std::vector<Matrix4x4<float>> skinning(static_cast<size_t>(numJoints));
    for (int i = 0; i < numJoints; ++i) {
        // skinMatrix = modelMatrix * inverse(restPoseModelMatrix)
        Matrix4x4<float> model = ozzToMatrix4x4(models[static_cast<size_t>(i)]);
        Matrix4x4<float> invBind = ozzToMatrix4x4(restModels[static_cast<size_t>(i)]).inverse();
        skinning[static_cast<size_t>(i)] = model * invBind;
    }

    return skinning;
}

// --- AnimationSystem (Flecs) ---

void registerAnimationSystem(flecs::world& world) {
    world.system<Skeleton, AnimationState, SkinningData>("AnimationSystem")
        .each([](flecs::entity /*entity*/, Skeleton& skel, AnimationState& state, SkinningData& skinning) {
            if (!skel.skeleton || !state.clip || !state.playing) {
                return;
            }

            const auto& skeleton = *skel.skeleton;
            const auto& clip = *state.clip;
            const float duration = clip.duration();

            // Advance time
            if (duration > 0.0f) {
                // Use world delta_time if available; fall back to 1/60
                float dt = 1.0f / 60.0f;
                state.time += dt * state.speed;

                if (state.loop) {
                    while (state.time > duration) {
                        state.time -= duration;
                    }
                    while (state.time < 0.0f) {
                        state.time += duration;
                    }
                } else {
                    if (state.time >= duration) {
                        state.time = duration;
                        state.playing = false;
                    }
                    if (state.time < 0.0f) {
                        state.time = 0.0f;
                        state.playing = false;
                    }
                }
            }

            // Compute ratio [0,1]
            float ratio = (duration > 0.0f) ? (state.time / duration) : 0.0f;

            // Sample, local-to-model, skinning
            AnimationSampler sampler;
            ozz::vector<ozz::math::SoaTransform> locals;
            sampler.sample(clip, skeleton, ratio, locals);

            ozz::vector<ozz::math::Float4x4> models;
            sampler.localToModel(skeleton, locals, models);

            auto matrices = sampler.computeSkinningMatrices(skeleton, models);

            // Write to SkinningData component
            skinning.jointMatrices.resize(matrices.size());
            for (size_t i = 0; i < matrices.size(); ++i) {
                std::copy(matrices[i].elements.begin(), matrices[i].elements.end(), skinning.jointMatrices[i].begin());
            }
        });
}

} // namespace fabric
