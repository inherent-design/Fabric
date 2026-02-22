#include "fabric/core/SceneView.hh"
#include "fabric/core/ECS.hh"
#include "fabric/core/Spatial.hh"
#include "fabric/utils/Profiler.hh"
#include <bgfx/bgfx.h>

namespace fabric {

SceneView::SceneView(uint8_t viewId, Camera& camera, flecs::world& world)
    : viewId_(viewId), camera_(camera), world_(world) {}

void SceneView::setClearColor(uint32_t rgba) {
    clearColor_ = rgba;
}

void SceneView::render() {
    FABRIC_ZONE_SCOPED_N("SceneView::render");

    // 1. Get VP matrix from camera
    float vp[16];
    camera_.getViewProjection(vp);

    // 2. Cull scene entities against frustum
    visibleEntities_ = FrustumCuller::cull(vp, world_);

    // 3. Build render list from visible entities
    renderList_.clear();
    for (auto entity : visibleEntities_) {
        DrawCall dc;
        dc.viewId = viewId_;

        // Build transform matrix from ECS components
        const auto* pos = entity.get<Position>();
        const auto* rot = entity.get<Rotation>();
        const auto* scl = entity.get<Scale>();

        Transform<float> t;
        if (pos) t.setPosition(Vector3<float, Space::World>(pos->x, pos->y, pos->z));
        if (rot) t.setRotation(Quaternion<float>(rot->x, rot->y, rot->z, rot->w));
        if (scl) t.setScale(Vector3<float, Space::World>(scl->x, scl->y, scl->z));

        auto matrix = t.getMatrix();
        for (int i = 0; i < 16; ++i) {
            dc.transform[static_cast<size_t>(i)] = matrix.elements[static_cast<size_t>(i)];
        }
        renderList_.addDrawCall(dc);
    }

    // 4. Set bgfx view transform and clear
    bgfx::setViewTransform(viewId_, camera_.viewMatrix(), camera_.projectionMatrix());
    bgfx::setViewClear(viewId_, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, clearColor_, 1.0f, 0);

    // 5. Ensure view is submitted even with no draw calls
    bgfx::touch(viewId_);
}

uint8_t SceneView::viewId() const {
    return viewId_;
}

Camera& SceneView::camera() {
    return camera_;
}

const std::vector<flecs::entity>& SceneView::visibleEntities() const {
    return visibleEntities_;
}

} // namespace fabric
