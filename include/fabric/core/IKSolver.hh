#pragma once

#include "fabric/core/Spatial.hh"
#include <cstdint>
#include <vector>

#include <ozz/base/containers/vector.h>
#include <ozz/base/maths/soa_transform.h>

namespace fabric {

using Vec3f = Vector3<float, Space::World>;
using Quatf = Quaternion<float>;

// Result of a two-bone IK solve (root-mid-tip chain).
// Contains local-space rotation corrections for the root and mid joints.
struct TwoBoneIKResult {
    Quatf rootCorrection;
    Quatf midCorrection;
    bool reached = false;
};

// Result of a FABRIK solve over a variable-length joint chain.
// Contains updated world-space positions for each joint in the chain.
struct FABRIKResult {
    std::vector<Vec3f> positions;
    int iterations = 0;
    bool converged = false;
};

// Analytical two-bone IK solver for 3-joint chains.
// Budget: 1-3us. No iteration required.
// Use cases: foot placement, hand reaching.
TwoBoneIKResult solveTwoBone(const Vec3f& root, const Vec3f& mid, const Vec3f& tip, const Vec3f& target,
                             const Vec3f& poleVector);

// FABRIK (Forward And Backward Reaching Inverse Kinematics) solver.
// Iterative solver for variable-length joint chains.
// Budget: 5-15us for 3-5 joint chains.
// Use cases: spine, look-at, tentacles.
FABRIKResult solveFABRIK(const std::vector<Vec3f>& chain, const Vec3f& target, float tolerance = 0.001f,
                         int maxIterations = 10);

// Derive rotation corrections from FABRIK position changes.
// Compares direction vectors between original and solved chains to produce
// per-joint quaternion corrections. The returned vector has (boneCount - 1)
// entries corresponding to joints 0..N-2 (the last joint has no child bone).
std::vector<Quatf> computeRotationsFromPositions(const std::vector<Vec3f>& oldPositions,
                                                 const std::vector<Vec3f>& newPositions);

// Apply an IK rotation correction to a specific joint in the ozz SoA
// local-space transform buffer. The jointIndex is the AoS joint index
// (not the SoA element index).
void applyIKToSkeleton(ozz::vector<ozz::math::SoaTransform>& locals, int jointIndex, const Quatf& rotation);

} // namespace fabric
