#pragma once

#include "fabric/render/DrawCall.hh"
#include "fabric/render/Geometry.hh"
#include "fabric/render/RenderCaps.hh"
#include "fabric/render/ViewLayout.hh"
#include <cstdint>
#include <flecs.h>
#include <functional>
#include <memory>
#include <vector>

namespace fabric {

// Forward declaration for BVH (defined in utils/BVH.hh)
template <typename T> class BVH;

// Frustum-cull scene entities against a view-projection matrix.
// Uses a BVH for O(log n) hierarchical pruning. Entities without BoundingBox
// are always considered visible (bypassing the BVH).
class FrustumCuller {
  public:
    FrustumCuller();
    ~FrustumCuller();

    FrustumCuller(const FrustumCuller&) = delete;
    FrustumCuller& operator=(const FrustumCuller&) = delete;
    FrustumCuller(FrustumCuller&&) noexcept;
    FrustumCuller& operator=(FrustumCuller&&) noexcept;

    // Build the BVH from all SceneEntity entities with BoundingBox in the world.
    // Call once per frame before cull().
    void buildSceneBVH(flecs::world& world);

    // Cull against the given view-projection matrix using the BVH.
    // Entities without BoundingBox (tracked separately) are always visible.
    std::vector<flecs::entity> cull(const float* viewProjection, flecs::world& world);

  private:
    std::unique_ptr<BVH<flecs::entity>> bvh_;
    std::vector<flecs::entity> alwaysVisible_; // entities without BoundingBox
};

// Sort entities back-to-front by distance from a camera position.
// Uses the entity's Position component (or LocalToWorld translation) as the center.
void transparentSort(std::vector<flecs::entity>& entities, const Vec3f& cameraPos);

// Global render capabilities, populated after bgfx::init().
const RenderCaps& renderCaps();

inline constexpr uint8_t K_OIT_ACCUM_VIEW_ID = render::view::K_OIT_ACCUM;
inline constexpr uint8_t K_OIT_COMPOSITE_VIEW_ID = render::view::K_OIT_COMPOSITE;

} // namespace fabric
