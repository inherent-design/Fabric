#include "fabric/core/FlightController.hh"
#include <algorithm>
#include <cmath>

namespace fabric {

FlightController::FlightController(float width, float height, float depth)
    : width_(width), height_(height), depth_(depth) {}

AABB FlightController::getAABB(const Vec3f& pos) const {
    float hw = width_ * 0.5f;
    float hd = depth_ * 0.5f;
    Vec3f minCorner(pos.x - hw, pos.y, pos.z - hd);
    Vec3f maxCorner(pos.x + hw, pos.y + height_, pos.z + hd);
    return AABB(minCorner, maxCorner);
}

bool FlightController::isSolid(int vx, int vy, int vz, const ChunkedGrid<float>& grid,
                                float threshold) const {
    return grid.get(vx, vy, vz) >= threshold;
}

bool FlightController::aabbOverlapsSolid(const AABB& box, const ChunkedGrid<float>& grid,
                                          float threshold) const {
    int minVX = static_cast<int>(std::floor(box.min.x));
    int minVY = static_cast<int>(std::floor(box.min.y));
    int minVZ = static_cast<int>(std::floor(box.min.z));
    int maxVX = static_cast<int>(std::floor(box.max.x - kEpsilon));
    int maxVY = static_cast<int>(std::floor(box.max.y - kEpsilon));
    int maxVZ = static_cast<int>(std::floor(box.max.z - kEpsilon));

    for (int vy = minVY; vy <= maxVY; ++vy)
        for (int vz = minVZ; vz <= maxVZ; ++vz)
            for (int vx = minVX; vx <= maxVX; ++vx)
                if (isSolid(vx, vy, vz, grid, threshold))
                    return true;
    return false;
}

Vec3f FlightController::applyDrag(const Vec3f& velocity, float dragCoefficient, float dt) {
    float factor = std::max(0.0f, 1.0f - dragCoefficient * dt);
    Vec3f result = velocity * factor;

    // Clamp near-zero to zero
    float speed = std::sqrt(result.x * result.x + result.y * result.y + result.z * result.z);
    if (speed < kDragFloor) {
        return Vec3f(0.0f, 0.0f, 0.0f);
    }
    return result;
}

FlightController::FlightResult FlightController::move(
    const Vec3f& currentPos, const Vec3f& displacement,
    const ChunkedGrid<float>& grid, float densityThreshold) {

    FlightResult result;
    Vec3f pos = currentPos;

    // All 3 axes resolved with equal priority (6DOF, no gravity bias)
    // X axis
    {
        Vec3f candidate(pos.x + displacement.x, pos.y, pos.z);
        AABB box = getAABB(candidate);
        if (aabbOverlapsSolid(box, grid, densityThreshold)) {
            result.hitX = true;
        } else {
            pos.x = candidate.x;
        }
    }

    // Y axis
    {
        Vec3f candidate(pos.x, pos.y + displacement.y, pos.z);
        AABB box = getAABB(candidate);
        if (aabbOverlapsSolid(box, grid, densityThreshold)) {
            result.hitY = true;
        } else {
            pos.y = candidate.y;
        }
    }

    // Z axis
    {
        Vec3f candidate(pos.x, pos.y, pos.z + displacement.z);
        AABB box = getAABB(candidate);
        if (aabbOverlapsSolid(box, grid, densityThreshold)) {
            result.hitZ = true;
        } else {
            pos.z = candidate.z;
        }
    }

    result.resolvedPosition = pos;
    return result;
}

} // namespace fabric
