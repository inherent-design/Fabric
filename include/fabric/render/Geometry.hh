#pragma once

#include "fabric/core/Spatial.hh"
#include <array>
#include <cstdint>

namespace fabric {

using Vec3f = Vector3<float, Space::World>;

// Axis-aligned bounding box
struct AABB {
    Vec3f min;
    Vec3f max;

    AABB();
    AABB(const Vec3f& min, const Vec3f& max);

    Vec3f center() const;
    Vec3f extents() const;

    void expand(const Vec3f& point);
    bool contains(const Vec3f& point) const;
    bool intersects(const AABB& other) const;
};

// Frustum plane (ax + by + cz + d = 0)
struct Plane {
    float a, b, c, d;

    float distanceToPoint(const Vec3f& point) const;
    void normalize();
};

enum class CullResult : std::uint8_t {
    Inside,
    Outside,
    Intersect
};

// View frustum (6 planes extracted from view-projection matrix)
struct Frustum {
    std::array<Plane, 6> planes; // left, right, bottom, top, near, far

    // Extract planes from a column-major 4x4 view-projection matrix
    void extractFromVP(const float* vp);

    CullResult testAABB(const AABB& aabb) const;
};

} // namespace fabric
