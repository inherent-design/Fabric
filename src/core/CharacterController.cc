#include "fabric/core/CharacterController.hh"
#include "fabric/core/Log.hh"
#include <algorithm>
#include <cmath>

namespace fabric {

CharacterController::CharacterController(float width, float height, float depth)
    : width_(width), height_(height), depth_(depth) {}

AABB CharacterController::getAABB(const Vec3f& pos) const {
    float hw = width_ * 0.5f;
    float hd = depth_ * 0.5f;
    Vec3f minCorner(pos.x - hw, pos.y, pos.z - hd);
    Vec3f maxCorner(pos.x + hw, pos.y + height_, pos.z + hd);
    return AABB(minCorner, maxCorner);
}

bool CharacterController::isSolid(int vx, int vy, int vz, const ChunkedGrid<float>& grid, float threshold) const {
    return grid.get(vx, vy, vz) >= threshold;
}

bool CharacterController::aabbOverlapsSolid(const AABB& box, const ChunkedGrid<float>& grid, float threshold) const {
    int minVX = static_cast<int>(std::floor(box.min.x));
    int minVY = static_cast<int>(std::floor(box.min.y));
    int minVZ = static_cast<int>(std::floor(box.min.z));
    int maxVX = static_cast<int>(std::floor(box.max.x - kGroundEpsilon));
    int maxVY = static_cast<int>(std::floor(box.max.y - kGroundEpsilon));
    int maxVZ = static_cast<int>(std::floor(box.max.z - kGroundEpsilon));

    for (int vy = minVY; vy <= maxVY; ++vy)
        for (int vz = minVZ; vz <= maxVZ; ++vz)
            for (int vx = minVX; vx <= maxVX; ++vx)
                if (isSolid(vx, vy, vz, grid, threshold))
                    return true;
    return false;
}

float CharacterController::tryStepUp(const Vec3f& pos, float dx, float dz, const ChunkedGrid<float>& grid,
                                     float threshold) const {
    // TODO(human): Implement the step-up resolution strategy
    // Try stepping up by increments up to stepHeight_ to clear obstacles.
    // Return the Y offset needed, or 0 if step-up is not possible.
    for (float step = 1.0f; step <= stepHeight_; step += 1.0f) {
        Vec3f stepped(pos.x + dx, pos.y + step, pos.z + dz);
        AABB box = getAABB(stepped);
        if (!aabbOverlapsSolid(box, grid, threshold)) {
            return step;
        }
    }
    return 0.0f;
}

CharacterController::CollisionResult CharacterController::move(const Vec3f& currentPos, const Vec3f& displacement,
                                                               const ChunkedGrid<float>& grid, float densityThreshold) {

    CollisionResult result;
    Vec3f pos = currentPos;

    // Resolve Y axis first (gravity/jump)
    {
        Vec3f candidate(pos.x, pos.y + displacement.y, pos.z);
        AABB box = getAABB(candidate);
        if (aabbOverlapsSolid(box, grid, densityThreshold)) {
            result.hitY = true;
            // Clamp: if moving down, push up to voxel top; if moving up, push down
            if (displacement.y < 0.0f) {
                // Find the highest blocking voxel below feet
                int footVY = static_cast<int>(std::floor(candidate.y));
                AABB footBox = getAABB(candidate);
                int minVX = static_cast<int>(std::floor(footBox.min.x));
                int maxVX = static_cast<int>(std::floor(footBox.max.x - kGroundEpsilon));
                int minVZ = static_cast<int>(std::floor(footBox.min.z));
                int maxVZ = static_cast<int>(std::floor(footBox.max.z - kGroundEpsilon));
                float highestTop = candidate.y;
                for (int vz = minVZ; vz <= maxVZ; ++vz) {
                    for (int vx = minVX; vx <= maxVX; ++vx) {
                        for (int vy = footVY; vy <= static_cast<int>(std::floor(pos.y)); ++vy) {
                            if (isSolid(vx, vy, vz, grid, densityThreshold)) {
                                float top = static_cast<float>(vy + 1);
                                if (top > highestTop)
                                    highestTop = top;
                            }
                        }
                    }
                }
                pos.y = highestTop;
            } else {
                // Moving up, clamp to below the blocking voxel
                // Stay put on Y; no vertical displacement allowed
            }
        } else {
            pos.y = candidate.y;
        }
    }

    // Resolve X axis
    {
        Vec3f candidate(pos.x + displacement.x, pos.y, pos.z);
        AABB box = getAABB(candidate);
        if (aabbOverlapsSolid(box, grid, densityThreshold)) {
            // Try step-up
            float stepOffset = tryStepUp(pos, displacement.x, 0.0f, grid, densityThreshold);
            if (stepOffset > 0.0f) {
                pos.x += displacement.x;
                pos.y += stepOffset;
            } else {
                result.hitX = true;
            }
        } else {
            pos.x = candidate.x;
        }
    }

    // Resolve Z axis
    {
        Vec3f candidate(pos.x, pos.y, pos.z + displacement.z);
        AABB box = getAABB(candidate);
        if (aabbOverlapsSolid(box, grid, densityThreshold)) {
            // Try step-up
            float stepOffset = tryStepUp(pos, 0.0f, displacement.z, grid, densityThreshold);
            if (stepOffset > 0.0f) {
                pos.z += displacement.z;
                pos.y += stepOffset;
            } else {
                result.hitZ = true;
            }
        } else {
            pos.z = candidate.z;
        }
    }

    result.onGround = checkOnGround(pos, grid, densityThreshold);
    result.resolvedPosition = pos;
    return result;
}

bool CharacterController::checkOnGround(const Vec3f& pos, const ChunkedGrid<float>& grid,
                                        float densityThreshold) const {
    // Check voxels directly below feet
    float checkY = pos.y - kGroundEpsilon;
    int vy = static_cast<int>(std::floor(checkY));

    float hw = width_ * 0.5f;
    float hd = depth_ * 0.5f;
    int minVX = static_cast<int>(std::floor(pos.x - hw));
    int maxVX = static_cast<int>(std::floor(pos.x + hw - kGroundEpsilon));
    int minVZ = static_cast<int>(std::floor(pos.z - hd));
    int maxVZ = static_cast<int>(std::floor(pos.z + hd - kGroundEpsilon));

    for (int vz = minVZ; vz <= maxVZ; ++vz)
        for (int vx = minVX; vx <= maxVX; ++vx)
            if (isSolid(vx, vy, vz, grid, densityThreshold))
                return true;
    return false;
}

float CharacterController::stepHeight() const {
    return stepHeight_;
}

void CharacterController::setStepHeight(float h) {
    stepHeight_ = h;
}

} // namespace fabric
