#include "fabric/core/Animation.hh"

#include "fabric/core/IKSolver.hh"
#include "fabric/core/Log.hh"
#include "fabric/core/VoxelRaycast.hh"
#include "fabric/utils/ErrorHandling.hh"
#include "fabric/utils/Profiler.hh"
#include <algorithm>
#include <cctype>
#include <cstring>
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

// --- JointMask ---

JointMask JointMask::createUpperBody(const ozz::animation::Skeleton& skeleton) {
    const int numJoints = skeleton.num_joints();
    const int numSoaJoints = skeleton.num_soa_joints();
    const auto names = skeleton.joint_names();
    const auto parents = skeleton.joint_parents();

    // AoS weights, then convert to SoA
    std::vector<float> perJoint(static_cast<size_t>(numJoints), 0.0f);

    // Find first joint containing "spine" (case-insensitive)
    int spineIndex = -1;
    for (int i = 0; i < numJoints; ++i) {
        std::string name(names[i]);
        std::string lower(name.size(), '\0');
        std::transform(name.begin(), name.end(), lower.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (lower.find("spine") != std::string::npos) {
            spineIndex = i;
            break;
        }
    }

    if (spineIndex >= 0) {
        // Mark spine and all descendants (parent < child in ozz ordering)
        std::vector<bool> upper(static_cast<size_t>(numJoints), false);
        upper[static_cast<size_t>(spineIndex)] = true;
        for (int i = spineIndex + 1; i < numJoints; ++i) {
            const int parent = parents[i];
            if (parent >= 0 && upper[static_cast<size_t>(parent)]) {
                upper[static_cast<size_t>(i)] = true;
            }
        }
        for (int i = 0; i < numJoints; ++i) {
            perJoint[static_cast<size_t>(i)] = upper[static_cast<size_t>(i)] ? 1.0f : 0.0f;
        }
    }

    // Convert AoS to SoA
    JointMask mask;
    mask.weights.resize(static_cast<size_t>(numSoaJoints));
    for (int i = 0; i < numSoaJoints; ++i) {
        const int base = i * 4;
        const float w0 = (base + 0 < numJoints) ? perJoint[static_cast<size_t>(base + 0)] : 0.0f;
        const float w1 = (base + 1 < numJoints) ? perJoint[static_cast<size_t>(base + 1)] : 0.0f;
        const float w2 = (base + 2 < numJoints) ? perJoint[static_cast<size_t>(base + 2)] : 0.0f;
        const float w3 = (base + 3 < numJoints) ? perJoint[static_cast<size_t>(base + 3)] : 0.0f;
        mask.weights[static_cast<size_t>(i)] = ozz::math::simd_float4::Load(w0, w1, w2, w3);
    }

    return mask;
}

JointMask JointMask::createFullBody(const ozz::animation::Skeleton& skeleton) {
    JointMask mask;
    const int numSoaJoints = skeleton.num_soa_joints();
    mask.weights.resize(static_cast<size_t>(numSoaJoints));
    for (int i = 0; i < numSoaJoints; ++i) {
        mask.weights[static_cast<size_t>(i)] = ozz::math::simd_float4::one();
    }
    return mask;
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

    // Cache inverse bind matrices per skeleton. They are constant for a given
    // skeleton, so we only recompute when the joint count changes (new skeleton).
    if (cachedSkeletonJointCount_ != numJoints) {
        const auto& restPoses = skeleton.joint_rest_poses();
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

        // Invert each rest-pose model matrix and store in cache
        cachedInvBindMatrices_.resize(static_cast<size_t>(numJoints));
        for (int i = 0; i < numJoints; ++i) {
            Matrix4x4<float> restModel = ozzToMatrix4x4(restModels[static_cast<size_t>(i)]);
            cachedInvBindMatrices_[static_cast<size_t>(i)] = matrix4x4ToOzz(restModel.inverse());
        }
        cachedSkeletonJointCount_ = numJoints;
    }

    std::vector<Matrix4x4<float>> skinning(static_cast<size_t>(numJoints));
    for (int i = 0; i < numJoints; ++i) {
        // skinMatrix = modelMatrix * cachedInverseBindMatrix
        skinning[static_cast<size_t>(i)] = ozzToMatrix4x4(models[static_cast<size_t>(i)]) *
                                           ozzToMatrix4x4(cachedInvBindMatrices_[static_cast<size_t>(i)]);
    }

    return skinning;
}

void AnimationSampler::sampleLayer(int layerIndex, const ozz::animation::Animation& clip,
                                   const ozz::animation::Skeleton& skeleton, float time,
                                   ozz::vector<ozz::math::SoaTransform>& locals) {
    FABRIC_ZONE_SCOPED;

    if (layerIndex < 0) {
        throwError("AnimationSampler::sampleLayer: negative layer index");
    }

    if (static_cast<size_t>(layerIndex) >= layerContexts_.size()) {
        layerContexts_.resize(static_cast<size_t>(layerIndex) + 1);
    }
    if (!layerContexts_[static_cast<size_t>(layerIndex)]) {
        layerContexts_[static_cast<size_t>(layerIndex)] = std::make_unique<ozz::animation::SamplingJob::Context>();
    }

    auto& ctx = *layerContexts_[static_cast<size_t>(layerIndex)];
    const int numSoaJoints = skeleton.num_soa_joints();
    locals.resize(static_cast<size_t>(numSoaJoints));

    if (ctx.max_tracks() < skeleton.num_joints()) {
        ctx.Resize(skeleton.num_joints());
    }

    ozz::animation::SamplingJob samplingJob;
    samplingJob.animation = &clip;
    samplingJob.context = &ctx;
    samplingJob.ratio = time;
    samplingJob.output = ozz::make_span(locals);

    if (!samplingJob.Run()) {
        throwError("AnimationSampler::sampleLayer: SamplingJob failed");
    }
}

void AnimationSampler::blendLayered(const ozz::animation::Skeleton& skeleton,
                                    ozz::span<const ozz::animation::BlendingJob::Layer> layers,
                                    ozz::vector<ozz::math::SoaTransform>& output) {
    FABRIC_ZONE_SCOPED;

    const int numSoaJoints = skeleton.num_soa_joints();
    output.resize(static_cast<size_t>(numSoaJoints));

    ozz::animation::BlendingJob blendJob;
    blendJob.layers = layers;
    blendJob.rest_pose = skeleton.joint_rest_poses();
    blendJob.output = ozz::make_span(output);
    blendJob.threshold = 0.1f;

    if (!blendJob.Run()) {
        throwError("AnimationSampler::blendLayered: BlendingJob failed");
    }
}

// --- Foot IK ---

void processFootIK(AnimationSampler& sampler, const ozz::animation::Skeleton& skeleton,
                   ozz::vector<ozz::math::SoaTransform>& locals, const FootIKConfig& config) {
    FABRIC_ZONE_SCOPED;

    if (!config.grounded || !config.grid) {
        return;
    }

    const int numJoints = skeleton.num_joints();

    auto validLeg = [numJoints](const FootIKLeg& leg) -> bool {
        return leg.hipJoint >= 0 && leg.hipJoint < numJoints && leg.kneeJoint >= 0 && leg.kneeJoint < numJoints &&
               leg.ankleJoint >= 0 && leg.ankleJoint < numJoints;
    };

    const bool hasLeft = validLeg(config.leftLeg);
    const bool hasRight = validLeg(config.rightLeg);
    if (!hasLeft && !hasRight) {
        return;
    }

    // Intermediate localToModel to get model-space joint positions
    ozz::vector<ozz::math::Float4x4> models;
    sampler.localToModel(skeleton, locals, models);

    using Vec3f = Vector3<float, Space::World>;

    auto extractPos = [](const ozz::math::Float4x4& m) -> Vec3f {
        alignas(16) float col3[4];
        ozz::math::StorePtrU(m.cols[3], col3);
        return Vec3f(col3[0], col3[1], col3[2]);
    };

    struct LegResult {
        Vec3f hip, knee, ankle;
        float groundModelY = 0.0f;
        bool hasGround = false;
    };

    auto castLeg = [&](const FootIKLeg& leg) -> LegResult {
        LegResult r;
        r.hip = extractPos(models[static_cast<size_t>(leg.hipJoint)]);
        r.knee = extractPos(models[static_cast<size_t>(leg.kneeJoint)]);
        r.ankle = extractPos(models[static_cast<size_t>(leg.ankleJoint)]);

        Vec3f ankleWorld = r.ankle + config.worldOffset;
        float rayOriginY = ankleWorld.y + config.raycastHeight;

        auto hit = castRay(*config.grid, ankleWorld.x, rayOriginY, ankleWorld.z, 0.0f, -1.0f, 0.0f,
                           config.raycastHeight + config.maxCorrectionDist);

        if (hit) {
            float groundWorldY = static_cast<float>(hit->y) + 1.0f;
            float groundModelY = groundWorldY - config.worldOffset.y;
            float correction = std::abs(r.ankle.y - (groundModelY + config.footHeightOffset));
            if (correction <= config.maxCorrectionDist) {
                r.groundModelY = groundModelY;
                r.hasGround = true;
            }
        }

        return r;
    };

    LegResult left{}, right{};
    if (hasLeft) {
        left = castLeg(config.leftLeg);
    }
    if (hasRight) {
        right = castLeg(config.rightLeg);
    }

    if (!left.hasGround && !right.hasGround) {
        return;
    }

    // Pelvis height adjustment: lower pelvis so lowest foot touches ground
    float pelvisOffset = 0.0f;
    if (left.hasGround && right.hasGround) {
        float leftDiff = (left.groundModelY + config.footHeightOffset) - left.ankle.y;
        float rightDiff = (right.groundModelY + config.footHeightOffset) - right.ankle.y;
        pelvisOffset = std::min(leftDiff, rightDiff);
    } else if (left.hasGround) {
        pelvisOffset = (left.groundModelY + config.footHeightOffset) - left.ankle.y;
    } else {
        pelvisOffset = (right.groundModelY + config.footHeightOffset) - right.ankle.y;
    }

    pelvisOffset = std::clamp(pelvisOffset, -config.maxCorrectionDist, config.maxCorrectionDist);

    // Apply pelvis offset to pelvis joint local Y translation
    {
        int soaIdx = config.pelvisJoint / 4;
        int lane = config.pelvisJoint % 4;

        if (static_cast<size_t>(soaIdx) < locals.size()) {
            alignas(16) float ty[4];
            ozz::math::StorePtrU(locals[static_cast<size_t>(soaIdx)].translation.y, ty);
            ty[lane] += pelvisOffset;
            locals[static_cast<size_t>(soaIdx)].translation.y =
                ozz::math::simd_float4::Load(ty[0], ty[1], ty[2], ty[3]);
        }
    }

    // Two-bone IK per leg
    Vec3f pelvisAdj(0.0f, pelvisOffset, 0.0f);

    auto applyLegIK = [&](const LegResult& leg, const FootIKLeg& legConfig) {
        if (!leg.hasGround) {
            return;
        }

        Vec3f adjHip = leg.hip + pelvisAdj;
        Vec3f adjKnee = leg.knee + pelvisAdj;
        Vec3f adjAnkle = leg.ankle + pelvisAdj;

        Vec3f target(adjAnkle.x, leg.groundModelY + config.footHeightOffset, adjAnkle.z);
        Vec3f poleVector = adjKnee + Vec3f(0.0f, 0.0f, 1.0f);

        auto ikResult = solveTwoBone(adjHip, adjKnee, adjAnkle, target, poleVector);
        applyIKToSkeleton(locals, legConfig.hipJoint, ikResult.rootCorrection);
        applyIKToSkeleton(locals, legConfig.kneeJoint, ikResult.midCorrection);
    };

    if (hasLeft) {
        applyLegIK(left, config.leftLeg);
    }
    if (hasRight) {
        applyLegIK(right, config.rightLeg);
    }
}

// --- Spine IK ---

void processSpineIK(AnimationSampler& sampler, const ozz::animation::Skeleton& skeleton,
                    ozz::vector<ozz::math::SoaTransform>& locals, const SpineIKConfig& config) {
    FABRIC_ZONE_SCOPED;

    if (config.jointIndices.size() < 2 || config.weight <= 0.0f) {
        return;
    }

    const int numJoints = skeleton.num_joints();
    for (int idx : config.jointIndices) {
        if (idx < 0 || idx >= numJoints) {
            return;
        }
    }

    // Build model-space transforms to extract spine joint positions
    ozz::vector<ozz::math::Float4x4> models;
    sampler.localToModel(skeleton, locals, models);

    using Vec3f = Vector3<float, Space::World>;

    auto extractPos = [](const ozz::math::Float4x4& m) -> Vec3f {
        alignas(16) float col3[4];
        ozz::math::StorePtrU(m.cols[3], col3);
        return Vec3f(col3[0], col3[1], col3[2]);
    };

    // Extract spine chain positions from model-space
    std::vector<Vec3f> chainPositions;
    chainPositions.reserve(config.jointIndices.size());
    for (int idx : config.jointIndices) {
        chainPositions.push_back(extractPos(models[static_cast<size_t>(idx)]));
    }

    // Run FABRIK solver
    auto fabrikResult = solveFABRIK(chainPositions, config.target, config.tolerance, config.maxIterations);

    // Convert position deltas to rotation corrections
    auto rotations = computeRotationsFromPositions(chainPositions, fabrikResult.positions);

    // Apply weighted, clamped rotation corrections to each joint
    Quatf identity;
    for (size_t i = 0; i < rotations.size(); ++i) {
        Quatf correction = rotations[i];

        // Per-joint angle clamping: limit the rotation magnitude
        if (config.maxAnglePerJoint > 0.0f) {
            // Extract rotation angle from quaternion (angle = 2 * acos(|w|))
            float halfAngle = std::acos(std::clamp(std::abs(correction.w), 0.0f, 1.0f));
            float angle = 2.0f * halfAngle;

            if (angle > config.maxAnglePerJoint) {
                // Scale down the rotation to the max angle
                float scale = config.maxAnglePerJoint / angle;
                correction = Quatf::slerp(identity, correction, scale);
            }
        }

        // Weight blending: slerp(identity, correction, weight)
        correction = Quatf::slerp(identity, correction, config.weight);

        applyIKToSkeleton(locals, config.jointIndices[i], correction);
    }
}

// --- AnimationSystem (Flecs) ---

void registerAnimationSystem(flecs::world& world) {
    world.system<Skeleton, AnimationState, SkinningData, AnimationSamplerComponent>("AnimationSystem")
        .without<AnimationLayerConfig>()
        .each([](flecs::entity entity, Skeleton& skel, AnimationState& state, SkinningData& skinning,
                 AnimationSamplerComponent& samplerComp) {
            if (!skel.skeleton || !state.clip || !state.playing) {
                return;
            }

            const auto& skeleton = *skel.skeleton;
            const auto& clip = *state.clip;
            const float duration = clip.duration();

            // Advance time
            if (duration > 0.0f) {
                float dt = entity.world().delta_time();
                if (dt <= 0.0f) {
                    dt = 1.0f / 60.0f; // fallback for first frame
                }
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

            // Sample, local-to-model, skinning (reuse per-entity sampler)
            auto& sampler = samplerComp.sampler;
            ozz::vector<ozz::math::SoaTransform> locals;
            sampler.sample(clip, skeleton, ratio, locals);

            // Foot IK: adjust foot placement on voxel terrain
            if (entity.has<FootIKConfig>()) {
                processFootIK(sampler, skeleton, locals, entity.get<FootIKConfig>());
            }

            // Spine IK: orient upper body toward aim target
            if (entity.has<SpineIKConfig>()) {
                processSpineIK(sampler, skeleton, locals, entity.get<SpineIKConfig>());
            }

            ozz::vector<ozz::math::Float4x4> models;
            sampler.localToModel(skeleton, locals, models);

            auto matrices = sampler.computeSkinningMatrices(skeleton, models);

            // Write to SkinningData component
            skinning.jointMatrices.resize(matrices.size());
            for (size_t i = 0; i < matrices.size(); ++i) {
                std::copy(matrices[i].elements.begin(), matrices[i].elements.end(), skinning.jointMatrices[i].begin());
            }
        });

    // Multi-layer system: layered blending with optional per-joint masks
    world.system<Skeleton, AnimationLayerConfig, SkinningData, AnimationSamplerComponent>("AnimationLayerSystem")
        .each([](flecs::entity entity, Skeleton& skel, AnimationLayerConfig& config, SkinningData& skinning,
                 AnimationSamplerComponent& samplerComp) {
            if (!skel.skeleton || config.layers.empty()) {
                return;
            }

            const auto& skeleton = *skel.skeleton;
            auto& sampler = samplerComp.sampler;

            float dt = entity.world().delta_time();
            if (dt <= 0.0f) {
                dt = 1.0f / 60.0f;
            }

            // Sample each layer and build BlendingJob layers
            const size_t numLayers = config.layers.size();
            std::vector<ozz::vector<ozz::math::SoaTransform>> layerLocals(numLayers);
            std::vector<ozz::animation::BlendingJob::Layer> blendLayers(numLayers);

            for (size_t i = 0; i < numLayers; ++i) {
                auto& layer = config.layers[i];

                if (!layer.state.clip || !layer.state.playing) {
                    blendLayers[i].weight = 0.0f;
                    blendLayers[i].transform = skeleton.joint_rest_poses();
                    continue;
                }

                // Advance time
                auto& state = layer.state;
                const float duration = state.clip->duration();
                if (duration > 0.0f) {
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

                const float ratio = (duration > 0.0f) ? (state.time / duration) : 0.0f;
                sampler.sampleLayer(static_cast<int>(i), *state.clip, skeleton, ratio, layerLocals[i]);

                blendLayers[i].weight = layer.weight;
                blendLayers[i].transform = ozz::make_span(layerLocals[i]);
                if (layer.mask) {
                    blendLayers[i].joint_weights = ozz::make_span(layer.mask->weights);
                }
            }

            // Blend all layers
            ozz::vector<ozz::math::SoaTransform> blended;
            sampler.blendLayered(
                skeleton, ozz::span<const ozz::animation::BlendingJob::Layer>(blendLayers.data(), blendLayers.size()),
                blended);

            // Foot IK: adjust foot placement on voxel terrain
            if (entity.has<FootIKConfig>()) {
                processFootIK(sampler, skeleton, blended, entity.get<FootIKConfig>());
            }

            // Spine IK: orient upper body toward aim target
            if (entity.has<SpineIKConfig>()) {
                processSpineIK(sampler, skeleton, blended, entity.get<SpineIKConfig>());
            }

            // Convert to model space and compute skinning matrices
            ozz::vector<ozz::math::Float4x4> models;
            sampler.localToModel(skeleton, blended, models);

            auto matrices = sampler.computeSkinningMatrices(skeleton, models);
            skinning.jointMatrices.resize(matrices.size());
            for (size_t i = 0; i < matrices.size(); ++i) {
                std::copy(matrices[i].elements.begin(), matrices[i].elements.end(), skinning.jointMatrices[i].begin());
            }
        });
}

} // namespace fabric
