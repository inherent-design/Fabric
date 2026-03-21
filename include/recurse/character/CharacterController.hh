#pragma once

#include "fabric/core/Spatial.hh"
#include "fabric/render/Geometry.hh"
#include "fabric/world/ChunkedGrid.hh"
#include "recurse/physics/VoxelCollision.hh"
#include <cmath>

namespace recurse {

// Engine types imported from fabric:: namespace
using fabric::AABB;
using fabric::ChunkedGrid;
using fabric::Vec3f;

class CharacterController {
  public:
    struct CollisionResult {
        bool hitX = false;
        bool hitY = false;
        bool hitZ = false;
        bool onGround = false;
        Vec3f resolvedPosition;
    };

    CharacterController(float width, float height, float depth);

    CollisionResult move(const Vec3f& currentPos, const Vec3f& displacement, const ChunkedGrid<float, 32>& grid,
                         float densityThreshold = 0.5f);

    bool checkOnGround(const Vec3f& pos, const ChunkedGrid<float, 32>& grid, float densityThreshold = 0.5f) const;

    AABB getAABB(const Vec3f& pos) const;

    float stepHeight() const;
    void setStepHeight(float h);

  private:
    float width_;
    float height_;
    float depth_;
    float stepHeight_ = 1.0f;

    static constexpr float K_GROUND_EPSILON = 0.01f;

    bool isSolid(int vx, int vy, int vz, const ChunkedGrid<float, 32>& grid, float threshold) const;

    bool aabbOverlapsSolid(const AABB& box, const ChunkedGrid<float, 32>& grid, float threshold) const;

    // TODO(human): Implement the step-up resolution strategy
    float tryStepUp(const Vec3f& pos, float dx, float dz, const ChunkedGrid<float, 32>& grid, float threshold) const;
};

} // namespace recurse
