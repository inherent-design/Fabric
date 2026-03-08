#include "recurse/gameplay/FlightController.hh"
#include "fabric/simulation/SimulationGrid.hh"
#include <algorithm>
#include <cmath>

using namespace fabric;

namespace recurse {

FlightController::FlightController(float width, float height, float depth)
    : width_(width), height_(height), depth_(depth) {}

AABB FlightController::getAABB(const Vec3f& pos) const {
    return physics::getAABB(pos, width_, height_, depth_);
}

bool FlightController::isSolid(int vx, int vy, int vz, const ChunkedGrid<float>& grid, float threshold) const {
    return physics::isSolid(vx, vy, vz, grid, threshold);
}

bool FlightController::aabbOverlapsSolid(const AABB& box, const ChunkedGrid<float>& grid, float threshold) const {
    return physics::aabbOverlapsSolid(box, grid, threshold, kEpsilon);
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

FlightController::FlightResult FlightController::move(const Vec3f& currentPos, const Vec3f& displacement,
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

bool FlightController::isSolid(int vx, int vy, int vz, const fabric::simulation::SimulationGrid& grid) const {
    return physics::isSolid(vx, vy, vz, grid);
}

bool FlightController::aabbOverlapsSolid(const AABB& box, const fabric::simulation::SimulationGrid& grid) const {
    return physics::aabbOverlapsSolid(box, grid, kEpsilon);
}

FlightController::FlightResult FlightController::move(const Vec3f& currentPos, const Vec3f& displacement,
                                                      const fabric::simulation::SimulationGrid& grid) {

    FlightResult result;
    Vec3f pos = currentPos;

    // X axis
    {
        Vec3f candidate(pos.x + displacement.x, pos.y, pos.z);
        AABB box = getAABB(candidate);
        if (aabbOverlapsSolid(box, grid)) {
            result.hitX = true;
        } else {
            pos.x = candidate.x;
        }
    }

    // Y axis
    {
        Vec3f candidate(pos.x, pos.y + displacement.y, pos.z);
        AABB box = getAABB(candidate);
        if (aabbOverlapsSolid(box, grid)) {
            result.hitY = true;
        } else {
            pos.y = candidate.y;
        }
    }

    // Z axis
    {
        Vec3f candidate(pos.x, pos.y, pos.z + displacement.z);
        AABB box = getAABB(candidate);
        if (aabbOverlapsSolid(box, grid)) {
            result.hitZ = true;
        } else {
            pos.z = candidate.z;
        }
    }

    result.resolvedPosition = pos;
    return result;
}

} // namespace recurse
