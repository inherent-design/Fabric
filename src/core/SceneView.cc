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
    uint8_t geoView = geometryViewId();
    renderList_.clear();
    for (auto entity : visibleEntities_) {
        DrawCall dc;
        dc.viewId = geoView;

        // Read pre-computed world transform from CASCADE system
        const auto* ltw = entity.try_get<LocalToWorld>();
        if (ltw) {
            dc.transform = ltw->matrix;
        } else {
            // Fallback: compose from components if LocalToWorld is missing
            Transform<float> t;
            const auto* pos = entity.try_get<Position>();
            const auto* rot = entity.try_get<Rotation>();
            const auto* scl = entity.try_get<Scale>();
            if (pos)
                t.setPosition(Vector3<float, Space::World>(pos->x, pos->y, pos->z));
            if (rot)
                t.setRotation(Quaternion<float>(rot->x, rot->y, rot->z, rot->w));
            if (scl)
                t.setScale(Vector3<float, Space::World>(scl->x, scl->y, scl->z));
            auto matrix = t.getMatrix();
            dc.transform = matrix.elements;
        }
        renderList_.addDrawCall(dc);
    }

    // 4. Sky view (viewId_): clear framebuffer and render atmospheric sky
    //    When post-process is active, scene renders into the HDR framebuffer.
    if (postProcess_.isValid()) {
        bgfx::setViewFrameBuffer(viewId_, postProcess_.hdrFramebuffer());
        bgfx::setViewFrameBuffer(geoView, postProcess_.hdrFramebuffer());
    }

    bgfx::setViewTransform(viewId_, camera_.viewMatrix(), camera_.projectionMatrix());
    bgfx::setViewClear(viewId_, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, clearColor_, 1.0f, 0);
    skyRenderer_.init();
    skyRenderer_.render(viewId_);
    bgfx::touch(viewId_);

    // 5. Geometry view (viewId_ + 1): no clear, inherits depth from sky view
    bgfx::setViewTransform(geoView, camera_.viewMatrix(), camera_.projectionMatrix());
    bgfx::setViewClear(geoView, BGFX_CLEAR_NONE);
    bgfx::touch(geoView);

    // 6. Post-process: bright extract -> blur -> tonemap to backbuffer
    if (postProcess_.isValid()) {
        postProcess_.render(200);
    }
}

uint8_t SceneView::viewId() const {
    return viewId_;
}

uint8_t SceneView::geometryViewId() const {
    return viewId_ + 1;
}

Camera& SceneView::camera() {
    return camera_;
}

SkyRenderer& SceneView::skyRenderer() {
    return skyRenderer_;
}

PostProcess& SceneView::postProcess() {
    return postProcess_;
}

const std::vector<flecs::entity>& SceneView::visibleEntities() const {
    return visibleEntities_;
}

void SceneView::enablePostProcess(uint16_t width, uint16_t height) {
    postProcess_.init(width, height);
}

} // namespace fabric
