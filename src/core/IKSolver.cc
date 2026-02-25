#include "fabric/core/IKSolver.hh"

#include "fabric/utils/Profiler.hh"
#include <algorithm>
#include <cmath>

#include <ozz/base/maths/simd_math.h>

namespace fabric {

namespace {

// Compute the angle at vertex B in triangle ABC using the law of cosines.
// Returns clamped angle in radians.
float triangleAngle(float a, float b, float c) {
    // a = side opposite A, b = side opposite B, c = side opposite C
    // cos(B) = (a^2 + c^2 - b^2) / (2*a*c)
    float denom = 2.0f * a * c;
    if (denom < 1e-8f) {
        return 0.0f;
    }
    float cosAngle = (a * a + c * c - b * b) / denom;
    cosAngle = std::clamp(cosAngle, -1.0f, 1.0f);
    return std::acos(cosAngle);
}

// Build a quaternion that rotates vector 'from' to vector 'to'.
Quatf rotationBetween(const Vec3f& from, const Vec3f& to) {
    Vec3f fn = from.normalized();
    Vec3f tn = to.normalized();

    float dot = fn.dot(tn);

    if (dot > 0.9999f) {
        return Quatf(); // identity
    }

    if (dot < -0.9999f) {
        // 180 degree rotation: pick an arbitrary perpendicular axis
        Vec3f perp = Vec3f(1.0f, 0.0f, 0.0f);
        if (std::abs(fn.dot(perp)) > 0.9f) {
            perp = Vec3f(0.0f, 1.0f, 0.0f);
        }
        Vec3f axis = fn.cross(perp).normalized();
        return Quatf(axis.x, axis.y, axis.z, 0.0f).normalized();
    }

    Vec3f axis = fn.cross(tn);
    float w = 1.0f + dot;
    return Quatf(axis.x, axis.y, axis.z, w).normalized();
}

} // namespace

TwoBoneIKResult solveTwoBone(const Vec3f& root, const Vec3f& mid, const Vec3f& tip, const Vec3f& target,
                             const Vec3f& poleVector) {
    FABRIC_ZONE_SCOPED;

    TwoBoneIKResult result;

    // Bone lengths
    float upperLen = (mid - root).length();
    float lowerLen = (tip - mid).length();
    float chainLen = upperLen + lowerLen;

    Vec3f toTarget = target - root;
    float targetDist = toTarget.length();

    // Handle degenerate cases
    if (targetDist < 1e-6f || upperLen < 1e-6f || lowerLen < 1e-6f) {
        result.rootCorrection = Quatf();
        result.midCorrection = Quatf();
        result.reached = false;
        return result;
    }

    // Clamp target distance to reachable range
    float clampedDist = std::clamp(targetDist, std::abs(upperLen - lowerLen) + 1e-4f, chainLen - 1e-4f);
    result.reached = (targetDist <= chainLen);

    // Desired direction from root to target
    Vec3f targetDir = toTarget.normalized();

    // Step 1: Root rotation to point toward target
    Vec3f currentDir = (tip - root).normalized();
    Quatf rootToTarget = rotationBetween(currentDir, targetDir);

    // Step 2: Compute elbow angle using law of cosines
    // Triangle: root-mid-tip with sides upper, lower, clampedDist
    float rootAngle = triangleAngle(clampedDist, lowerLen, upperLen);
    float midAngle = triangleAngle(upperLen, clampedDist, lowerLen);

    // Current angles in the chain
    Vec3f rootToMid = (mid - root).normalized();
    Vec3f midToTip = (tip - mid).normalized();
    float currentRootAngle = std::acos(std::clamp(rootToMid.dot(targetDir), -1.0f, 1.0f));
    float currentMidAngle = std::acos(std::clamp((rootToMid * -1.0f).dot(midToTip), -1.0f, 1.0f));

    // Step 3: Apply pole vector constraint
    // Project pole vector onto the plane perpendicular to root-to-target
    Vec3f poleProjDir = poleVector - root;
    float poleProj = poleProjDir.dot(targetDir);
    Vec3f poleOnPlane = (poleProjDir - targetDir * poleProj);
    if (poleOnPlane.length() > 1e-6f) {
        poleOnPlane = poleOnPlane.normalized();
    }

    // Build root correction: rotate toward target, then adjust for elbow angle
    float rootAngleDelta = rootAngle - currentRootAngle;
    Vec3f rootRotAxis = rootToMid.cross(targetDir);
    if (rootRotAxis.length() < 1e-6f) {
        rootRotAxis = poleOnPlane;
    }
    rootRotAxis = rootRotAxis.normalized();

    result.rootCorrection = rootToTarget;

    // Mid correction: adjust the bend angle
    float midAngleDelta = midAngle - currentMidAngle;
    Vec3f midRotAxis = rootToMid.cross(midToTip);
    if (midRotAxis.length() < 1e-6f) {
        midRotAxis = poleOnPlane;
        if (midRotAxis.length() < 1e-6f) {
            midRotAxis = Vec3f(0.0f, 0.0f, 1.0f);
        }
    }
    midRotAxis = midRotAxis.normalized();

    result.midCorrection = Quaternion<float>::fromAxisAngle(midRotAxis, midAngleDelta);

    return result;
}

FABRIKResult solveFABRIK(const std::vector<Vec3f>& chain, const Vec3f& target, float tolerance, int maxIterations) {
    FABRIC_ZONE_SCOPED;

    FABRIKResult result;
    result.positions = chain;
    result.iterations = 0;
    result.converged = false;

    if (chain.size() < 2) {
        result.converged = true;
        return result;
    }

    const size_t n = chain.size();

    // Precompute bone lengths
    std::vector<float> lengths(n - 1);
    float totalLength = 0.0f;
    for (size_t i = 0; i < n - 1; ++i) {
        lengths[i] = (chain[i + 1] - chain[i]).length();
        totalLength += lengths[i];
    }

    // Check if target is reachable
    float rootToTarget = (target - chain[0]).length();
    if (rootToTarget > totalLength) {
        // Target unreachable: straighten chain toward target
        Vec3f dir = (target - chain[0]).normalized();
        for (size_t i = 1; i < n; ++i) {
            result.positions[i] = result.positions[i - 1] + dir * lengths[i - 1];
        }
        result.iterations = 1;
        result.converged = false;
        return result;
    }

    Vec3f rootPos = chain[0];

    for (int iter = 0; iter < maxIterations; ++iter) {
        result.iterations = iter + 1;

        // Check convergence
        float endToTarget = (result.positions[n - 1] - target).length();
        if (endToTarget <= tolerance) {
            result.converged = true;
            break;
        }

        // Forward pass: move end effector to target, work backward
        result.positions[n - 1] = target;
        for (size_t i = n - 2; i < n; --i) {
            Vec3f dir = (result.positions[i] - result.positions[i + 1]);
            float dirLen = dir.length();
            if (dirLen > 1e-8f) {
                dir = dir * (1.0f / dirLen);
            } else {
                dir = Vec3f(0.0f, 1.0f, 0.0f);
            }
            result.positions[i] = result.positions[i + 1] + dir * lengths[i];
        }

        // Backward pass: fix root position, work forward
        result.positions[0] = rootPos;
        for (size_t i = 0; i < n - 1; ++i) {
            Vec3f dir = (result.positions[i + 1] - result.positions[i]);
            float dirLen = dir.length();
            if (dirLen > 1e-8f) {
                dir = dir * (1.0f / dirLen);
            } else {
                dir = Vec3f(0.0f, 1.0f, 0.0f);
            }
            result.positions[i + 1] = result.positions[i] + dir * lengths[i];
        }
    }

    // Final convergence check
    if (!result.converged) {
        float endToTarget = (result.positions[n - 1] - target).length();
        result.converged = (endToTarget <= tolerance);
    }

    return result;
}

void applyIKToSkeleton(ozz::vector<ozz::math::SoaTransform>& locals, int jointIndex, const Quatf& rotation) {
    FABRIC_ZONE_SCOPED;

    if (jointIndex < 0) {
        return;
    }

    // ozz SoA packs 4 joints per SoaTransform element
    int soaIndex = jointIndex / 4;
    int laneIndex = jointIndex % 4;

    if (static_cast<size_t>(soaIndex) >= locals.size()) {
        return;
    }

    auto& soa = locals[static_cast<size_t>(soaIndex)];

    // Extract the current quaternion from the SoA lane
    alignas(16) float qx[4], qy[4], qz[4], qw[4];
    ozz::math::StorePtrU(soa.rotation.x, qx);
    ozz::math::StorePtrU(soa.rotation.y, qy);
    ozz::math::StorePtrU(soa.rotation.z, qz);
    ozz::math::StorePtrU(soa.rotation.w, qw);

    // Read current rotation for this joint
    Quatf current(qx[laneIndex], qy[laneIndex], qz[laneIndex], qw[laneIndex]);

    // Apply correction: new = correction * current
    Quatf corrected = (rotation * current).normalized();

    // Write back
    qx[laneIndex] = corrected.x;
    qy[laneIndex] = corrected.y;
    qz[laneIndex] = corrected.z;
    qw[laneIndex] = corrected.w;

    soa.rotation.x = ozz::math::simd_float4::Load(qx[0], qx[1], qx[2], qx[3]);
    soa.rotation.y = ozz::math::simd_float4::Load(qy[0], qy[1], qy[2], qy[3]);
    soa.rotation.z = ozz::math::simd_float4::Load(qz[0], qz[1], qz[2], qz[3]);
    soa.rotation.w = ozz::math::simd_float4::Load(qw[0], qw[1], qw[2], qw[3]);
}

} // namespace fabric
