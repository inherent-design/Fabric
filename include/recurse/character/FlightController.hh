#pragma once

#include "fabric/core/Rendering.hh"
#include "fabric/core/Spatial.hh"
#include "fabric/world/ChunkedGrid.hh"
#include "recurse/physics/VoxelCollision.hh"

namespace recurse::simulation {
class SimulationGrid;
} // namespace recurse::simulation

namespace recurse {

// Engine types imported from fabric:: namespace
using fabric::AABB;
using fabric::ChunkedGrid;
using fabric::Vec3f;

class FlightController {
  public:
    struct FlightResult {
        Vec3f resolvedPosition;
        bool hitX = false;
        bool hitY = false;
        bool hitZ = false;
    };

    FlightController(float width, float height, float depth);

    // 6DOF collision resolution: displacement in world space, no gravity, no step-up
    FlightResult move(const Vec3f& currentPos, const Vec3f& displacement, const ChunkedGrid<float>& grid,
                      float densityThreshold = 0.5f);

    FlightResult move(const Vec3f& currentPos, const Vec3f& displacement,
                      const recurse::simulation::SimulationGrid& grid);

    // Drag utility: velocity *= (1 - drag * dt), clamps near-zero
    static Vec3f applyDrag(const Vec3f& velocity, float dragCoefficient, float dt);

    AABB getAABB(const Vec3f& pos) const;

  private:
    float width_;
    float height_;
    float depth_;

    static constexpr float K_EPSILON = 0.01f;
    static constexpr float K_DRAG_FLOOR = 0.01f;

    bool isSolid(int vx, int vy, int vz, const ChunkedGrid<float>& grid, float threshold) const;

    bool aabbOverlapsSolid(const AABB& box, const ChunkedGrid<float>& grid, float threshold) const;

    bool isSolid(int vx, int vy, int vz, const recurse::simulation::SimulationGrid& grid) const;

    bool aabbOverlapsSolid(const AABB& box, const recurse::simulation::SimulationGrid& grid) const;
};

} // namespace recurse
