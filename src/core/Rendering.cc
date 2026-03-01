#include "fabric/core/Rendering.hh"
#include "fabric/core/ECS.hh"
#include "fabric/utils/BVH.hh"
#include "fabric/utils/Profiler.hh"
#include <cmath>

namespace fabric {

// Global RenderCaps singleton
static RenderCaps s_renderCaps;
static bool s_renderCapsInitialized = false;

const RenderCaps& renderCaps() {
    if (!s_renderCapsInitialized) {
        s_renderCaps.initFromBgfx();
        s_renderCapsInitialized = true;
    }
    return s_renderCaps;
}

// AABB

AABB::AABB() : min(Vec3f(0.0f, 0.0f, 0.0f)), max(Vec3f(0.0f, 0.0f, 0.0f)) {}

AABB::AABB(const Vec3f& min, const Vec3f& max) : min(min), max(max) {}

Vec3f AABB::center() const {
    return Vec3f((min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f, (min.z + max.z) * 0.5f);
}

Vec3f AABB::extents() const {
    return Vec3f((max.x - min.x) * 0.5f, (max.y - min.y) * 0.5f, (max.z - min.z) * 0.5f);
}

void AABB::expand(const Vec3f& point) {
    min = Vec3f(std::min(min.x, point.x), std::min(min.y, point.y), std::min(min.z, point.z));
    max = Vec3f(std::max(max.x, point.x), std::max(max.y, point.y), std::max(max.z, point.z));
}

bool AABB::contains(const Vec3f& point) const {
    return point.x >= min.x && point.x <= max.x && point.y >= min.y && point.y <= max.y && point.z >= min.z &&
           point.z <= max.z;
}

bool AABB::intersects(const AABB& other) const {
    return min.x <= other.max.x && max.x >= other.min.x && min.y <= other.max.y && max.y >= other.min.y &&
           min.z <= other.max.z && max.z >= other.min.z;
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
    auto row = [&](int r, int c) {
        return vp[c * 4 + r];
    };

    // Left:   row3 + row0
    planes[0] = {row(3, 0) + row(0, 0), row(3, 1) + row(0, 1), row(3, 2) + row(0, 2), row(3, 3) + row(0, 3)};
    // Right:  row3 - row0
    planes[1] = {row(3, 0) - row(0, 0), row(3, 1) - row(0, 1), row(3, 2) - row(0, 2), row(3, 3) - row(0, 3)};
    // Bottom: row3 + row1
    planes[2] = {row(3, 0) + row(1, 0), row(3, 1) + row(1, 1), row(3, 2) + row(1, 2), row(3, 3) + row(1, 3)};
    // Top:    row3 - row1
    planes[3] = {row(3, 0) - row(1, 0), row(3, 1) - row(1, 1), row(3, 2) - row(1, 2), row(3, 3) - row(1, 3)};
    // Near:   row3 + row2
    planes[4] = {row(3, 0) + row(2, 0), row(3, 1) + row(2, 1), row(3, 2) + row(2, 2), row(3, 3) + row(2, 3)};
    // Far:    row3 - row2
    planes[5] = {row(3, 0) - row(2, 0), row(3, 1) - row(2, 1), row(3, 2) - row(2, 2), row(3, 3) - row(2, 3)};

    for (auto& plane : planes) {
        plane.normalize();
    }
}

CullResult Frustum::testAABB(const AABB& aabb) const {
    bool allInside = true;

    for (const auto& plane : planes) {
        // Find the positive and negative vertices relative to the plane normal
        Vec3f pVertex(plane.a >= 0.0f ? aabb.max.x : aabb.min.x, plane.b >= 0.0f ? aabb.max.y : aabb.min.y,
                      plane.c >= 0.0f ? aabb.max.z : aabb.min.z);
        Vec3f nVertex(plane.a >= 0.0f ? aabb.min.x : aabb.max.x, plane.b >= 0.0f ? aabb.min.y : aabb.max.y,
                      plane.c >= 0.0f ? aabb.min.z : aabb.max.z);

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
              [](const DrawCall& a, const DrawCall& b) { return a.sortKey < b.sortKey; });
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

Transform<float> TransformInterpolator::interpolate(const Transform<float>& prev, const Transform<float>& current,
                                                    float alpha) {
    Transform<float> result;

    result.setPosition(Vector3<float, Space::World>::lerp(prev.getPosition(), current.getPosition(), alpha));

    result.setRotation(Quaternion<float>::slerp(prev.getRotation(), current.getRotation(), alpha));

    result.setScale(Vector3<float, Space::World>::lerp(prev.getScale(), current.getScale(), alpha));

    return result;
}

// FrustumCuller

FrustumCuller::FrustumCuller() : bvh_(std::make_unique<BVH<flecs::entity>>()) {}

FrustumCuller::~FrustumCuller() = default;

FrustumCuller::FrustumCuller(FrustumCuller&&) noexcept = default;
FrustumCuller& FrustumCuller::operator=(FrustumCuller&&) noexcept = default;

static AABB computeWorldAABB(const BoundingBox& bb, const LocalToWorld* ltw) {
    AABB localAABB(Vec3f(bb.minX, bb.minY, bb.minZ), Vec3f(bb.maxX, bb.maxY, bb.maxZ));

    if (ltw) {
        Matrix4x4<float> m(ltw->matrix);
        AABB worldAABB;
        bool first = true;
        for (int cx = 0; cx < 2; ++cx) {
            for (int cy = 0; cy < 2; ++cy) {
                for (int cz = 0; cz < 2; ++cz) {
                    Vec3f corner(cx == 0 ? localAABB.min.x : localAABB.max.x,
                                 cy == 0 ? localAABB.min.y : localAABB.max.y,
                                 cz == 0 ? localAABB.min.z : localAABB.max.z);
                    auto worldCorner = m.transformPoint<Space::World, Space::World>(corner);
                    if (first) {
                        worldAABB = AABB(worldCorner, worldCorner);
                        first = false;
                    } else {
                        worldAABB.expand(worldCorner);
                    }
                }
            }
        }
        return worldAABB;
    }

    return localAABB;
}

void FrustumCuller::buildSceneBVH(flecs::world& world) {
    FABRIC_ZONE_SCOPED_N("FrustumCuller::buildSceneBVH");

    bvh_->clear();
    alwaysVisible_.clear();

    world.each([&](flecs::entity e, const Position&) {
        if (!e.has<SceneEntity>())
            return;

        const auto* bb = e.try_get<BoundingBox>();
        if (bb) {
            AABB worldAABB = computeWorldAABB(*bb, e.try_get<LocalToWorld>());
            bvh_->insert(worldAABB, e);
        } else {
            alwaysVisible_.push_back(e);
        }
    });

    bvh_->build();
}

std::vector<flecs::entity> FrustumCuller::cull(const float* viewProjection, flecs::world& world) {
    FABRIC_ZONE_SCOPED_N("FrustumCuller::cull");

    buildSceneBVH(world);

    Frustum frustum;
    frustum.extractFromVP(viewProjection);

    // Query BVH for entities with BoundingBox that pass the frustum test
    std::vector<flecs::entity> visible = bvh_->queryFrustum(frustum);

    // Append entities without BoundingBox (always visible)
    visible.insert(visible.end(), alwaysVisible_.begin(), alwaysVisible_.end());

    return visible;
}

// transparentSort

static Vec3f entityCenter(flecs::entity e) {
    // Prefer Position component (always up-to-date) over LocalToWorld
    // (which requires updateTransforms() to be current).
    const auto* pos = e.try_get<Position>();
    if (pos) {
        return Vec3f(pos->x, pos->y, pos->z);
    }
    const auto* ltw = e.try_get<LocalToWorld>();
    if (ltw) {
        // Extract translation from column 3 of the 4x4 matrix (column-major)
        return Vec3f(ltw->matrix[12], ltw->matrix[13], ltw->matrix[14]);
    }
    return Vec3f(0.0f, 0.0f, 0.0f);
}

void transparentSort(std::vector<flecs::entity>& entities, const Vec3f& cameraPos) {
    FABRIC_ZONE_SCOPED_N("transparentSort");

    // Sort back-to-front: entities farther from camera come first
    std::sort(entities.begin(), entities.end(), [&](flecs::entity a, flecs::entity b) {
        Vec3f ca = entityCenter(a);
        Vec3f cb = entityCenter(b);
        float da = (ca - cameraPos).lengthSquared();
        float db = (cb - cameraPos).lengthSquared();
        return da > db; // back-to-front: farther first
    });
}

} // namespace fabric
