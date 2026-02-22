#pragma once

#include <vector>
#include <array>
#include <cstdint>
#include <algorithm>
#include "fabric/core/Spatial.hh"

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

enum class CullResult {
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

// Draw call for bgfx submission
struct DrawCall {
    uint64_t sortKey = 0;
    std::array<float, 16> transform = {};
    // bgfx handles stored as uint16_t to avoid bgfx header dependency
    uint16_t program = 0;
    uint16_t vertexBuffer = 0;
    uint16_t indexBuffer = 0;
    uint32_t indexCount = 0;
    uint32_t indexOffset = 0;
    uint8_t viewId = 0;
};

// Sorted collection of draw calls per view
class RenderList {
public:
    void addDrawCall(const DrawCall& call);
    void sortByKey();
    void clear();

    const std::vector<DrawCall>& drawCalls() const;
    size_t size() const;
    bool empty() const;

private:
    std::vector<DrawCall> drawCalls_;
};

// Transform interpolation using slerp (rotation) + lerp (position, scale)
struct TransformInterpolator {
    static Transform<float> interpolate(
        const Transform<float>& prev,
        const Transform<float>& current,
        float alpha
    );
};

} // namespace fabric
