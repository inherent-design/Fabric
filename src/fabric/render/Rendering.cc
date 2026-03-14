#include "fabric/render/Rendering.hh"
#include "fabric/ecs/ECS.hh"
#include "fabric/utils/BVH.hh"
#include "fabric/utils/Profiler.hh"
#include <algorithm>

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

    bvh_->beginBatch();
    world.each([&](flecs::entity e, const SceneEntity&) {
        const auto* bb = e.try_get<BoundingBox>();
        if (bb) {
            AABB worldAABB = computeWorldAABB(*bb, e.try_get<LocalToWorld>());
            bvh_->insert(worldAABB, e);
        } else {
            alwaysVisible_.push_back(e);
        }
    });
    bvh_->commitBatch();
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
