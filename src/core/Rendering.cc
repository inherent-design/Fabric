#include "fabric/core/Rendering.hh"
#include "fabric/core/ECS.hh"
#include "fabric/utils/Profiler.hh"
#include <cmath>

namespace fabric {

// AABB

AABB::AABB()
    : min(Vec3f(0.0f, 0.0f, 0.0f)),
      max(Vec3f(0.0f, 0.0f, 0.0f)) {}

AABB::AABB(const Vec3f& min, const Vec3f& max)
    : min(min), max(max) {}

Vec3f AABB::center() const {
    return Vec3f(
        (min.x + max.x) * 0.5f,
        (min.y + max.y) * 0.5f,
        (min.z + max.z) * 0.5f
    );
}

Vec3f AABB::extents() const {
    return Vec3f(
        (max.x - min.x) * 0.5f,
        (max.y - min.y) * 0.5f,
        (max.z - min.z) * 0.5f
    );
}

void AABB::expand(const Vec3f& point) {
    min = Vec3f(
        std::min(min.x, point.x),
        std::min(min.y, point.y),
        std::min(min.z, point.z)
    );
    max = Vec3f(
        std::max(max.x, point.x),
        std::max(max.y, point.y),
        std::max(max.z, point.z)
    );
}

bool AABB::contains(const Vec3f& point) const {
    return point.x >= min.x && point.x <= max.x
        && point.y >= min.y && point.y <= max.y
        && point.z >= min.z && point.z <= max.z;
}

bool AABB::intersects(const AABB& other) const {
    return min.x <= other.max.x && max.x >= other.min.x
        && min.y <= other.max.y && max.y >= other.min.y
        && min.z <= other.max.z && max.z >= other.min.z;
}

// Plane

float Plane::distanceToPoint(const Vec3f& point) const {
    return a * point.x + b * point.y + c * point.z + d;
}

void Plane::normalize() {
    float len = std::sqrt(a * a + b * b + c * c);
    if (len > 0.0f) {
        float invLen = 1.0f / len;
        a *= invLen;
        b *= invLen;
        c *= invLen;
        d *= invLen;
    }
}

// Frustum - Gribb/Hartmann method: extract planes from VP matrix rows

void Frustum::extractFromVP(const float* vp) {
    // Column-major access: element at row r, col c = vp[c * 4 + r]
    // Row vectors of the VP matrix (transposed access)
    auto row = [&](int r, int c) { return vp[c * 4 + r]; };

    // Left:   row3 + row0
    planes[0] = {row(3,0) + row(0,0), row(3,1) + row(0,1), row(3,2) + row(0,2), row(3,3) + row(0,3)};
    // Right:  row3 - row0
    planes[1] = {row(3,0) - row(0,0), row(3,1) - row(0,1), row(3,2) - row(0,2), row(3,3) - row(0,3)};
    // Bottom: row3 + row1
    planes[2] = {row(3,0) + row(1,0), row(3,1) + row(1,1), row(3,2) + row(1,2), row(3,3) + row(1,3)};
    // Top:    row3 - row1
    planes[3] = {row(3,0) - row(1,0), row(3,1) - row(1,1), row(3,2) - row(1,2), row(3,3) - row(1,3)};
    // Near:   row3 + row2
    planes[4] = {row(3,0) + row(2,0), row(3,1) + row(2,1), row(3,2) + row(2,2), row(3,3) + row(2,3)};
    // Far:    row3 - row2
    planes[5] = {row(3,0) - row(2,0), row(3,1) - row(2,1), row(3,2) - row(2,2), row(3,3) - row(2,3)};

    for (auto& plane : planes) {
        plane.normalize();
    }
}

CullResult Frustum::testAABB(const AABB& aabb) const {
    bool allInside = true;

    for (const auto& plane : planes) {
        // Find the positive and negative vertices relative to the plane normal
        Vec3f pVertex(
            plane.a >= 0.0f ? aabb.max.x : aabb.min.x,
            plane.b >= 0.0f ? aabb.max.y : aabb.min.y,
            plane.c >= 0.0f ? aabb.max.z : aabb.min.z
        );
        Vec3f nVertex(
            plane.a >= 0.0f ? aabb.min.x : aabb.max.x,
            plane.b >= 0.0f ? aabb.min.y : aabb.max.y,
            plane.c >= 0.0f ? aabb.min.z : aabb.max.z
        );

        if (plane.distanceToPoint(pVertex) < 0.0f) {
            return CullResult::Outside;
        }
        if (plane.distanceToPoint(nVertex) < 0.0f) {
            allInside = false;
        }
    }

    return allInside ? CullResult::Inside : CullResult::Intersect;
}

// RenderList

void RenderList::addDrawCall(const DrawCall& call) {
    drawCalls_.push_back(call);
}

void RenderList::sortByKey() {
    FABRIC_ZONE_SCOPED_N("RenderList::sortByKey");
    std::sort(drawCalls_.begin(), drawCalls_.end(),
        [](const DrawCall& a, const DrawCall& b) {
            return a.sortKey < b.sortKey;
        });
}

void RenderList::clear() {
    drawCalls_.clear();
}

const std::vector<DrawCall>& RenderList::drawCalls() const {
    return drawCalls_;
}

size_t RenderList::size() const {
    return drawCalls_.size();
}

bool RenderList::empty() const {
    return drawCalls_.empty();
}

// TransformInterpolator

Transform<float> TransformInterpolator::interpolate(
    const Transform<float>& prev,
    const Transform<float>& current,
    float alpha
) {
    Transform<float> result;

    result.setPosition(Vector3<float, Space::World>::lerp(
        prev.getPosition(), current.getPosition(), alpha));

    result.setRotation(Quaternion<float>::slerp(
        prev.getRotation(), current.getRotation(), alpha));

    result.setScale(Vector3<float, Space::World>::lerp(
        prev.getScale(), current.getScale(), alpha));

    return result;
}

// FrustumCuller

std::vector<flecs::entity> FrustumCuller::cull(
    const float* viewProjection,
    flecs::world& world
) {
    FABRIC_ZONE_SCOPED_N("FrustumCuller::cull");

    Frustum frustum;
    frustum.extractFromVP(viewProjection);

    std::vector<flecs::entity> visible;

    // Flat iteration: test each SceneEntity independently
    world.each([&](flecs::entity e, const Position&) {
        if (!e.has<SceneEntity>()) return;

        const auto* bb = e.get<BoundingBox>();
        if (bb) {
            AABB aabb(
                Vec3f(bb->minX, bb->minY, bb->minZ),
                Vec3f(bb->maxX, bb->maxY, bb->maxZ)
            );
            if (frustum.testAABB(aabb) == CullResult::Outside) return;
        }

        visible.push_back(e);
    });

    return visible;
}

} // namespace fabric
