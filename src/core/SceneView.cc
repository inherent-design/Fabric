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

void SceneView::buildDrawCall(flecs::entity entity, uint8_t viewId, RenderList& list) {
    DrawCall dc;
    dc.viewId = viewId;

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
    list.addDrawCall(dc);
}

void SceneView::render() {
    FABRIC_ZONE_SCOPED_N("SceneView::render");

    // 1. Get VP matrix from camera
    float vp[16];
    camera_.getViewProjection(vp);

    // 2. Cull scene entities against frustum (BVH-accelerated)
    visibleEntities_ = frustumCuller_.cull(vp, world_);

    // 3. Partition visible entities into opaque and transparent lists
    uint8_t geoView = geometryViewId();
    opaqueEntities_.clear();
    transparentEntities_.clear();
    for (auto entity : visibleEntities_) {
        if (entity.has<TransparentTag>()) {
            transparentEntities_.push_back(entity);
        } else {
            opaqueEntities_.push_back(entity);
        }
    }

    // 4. Sort transparent entities back-to-front
    if (!transparentEntities_.empty()) {
        Vec3f camPos = camera_.getPosition();
        transparentSort(transparentEntities_, camPos);
    }

    // 5. Build render lists
    renderList_.clear();
    for (auto entity : opaqueEntities_) {
        buildDrawCall(entity, geoView, renderList_);
    }

    transparentRenderList_.clear();
    for (auto entity : transparentEntities_) {
        buildDrawCall(entity, transparentViewId(), transparentRenderList_);
    }

    // 6. Sky view (viewId_): clear framebuffer and render atmospheric sky
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

    // 7. Geometry view (viewId_ + 1): no clear, inherits depth from sky view
    bgfx::setViewTransform(geoView, camera_.viewMatrix(), camera_.projectionMatrix());
    bgfx::setViewClear(geoView, BGFX_CLEAR_NONE);
    bgfx::touch(geoView);

    // 8. Transparent view (viewId_ + 2): no depth write, alpha blend, back-to-front
    if (!transparentEntities_.empty()) {
        if (postProcess_.isValid()) {
            bgfx::setViewFrameBuffer(transparentViewId(), postProcess_.hdrFramebuffer());
        }
        bgfx::setViewTransform(transparentViewId(), camera_.viewMatrix(), camera_.projectionMatrix());
        // No clear on transparent pass: it composites over the geometry pass
        bgfx::setViewClear(transparentViewId(), BGFX_CLEAR_NONE);
        bgfx::touch(transparentViewId());
    }

    // 9. Post-process: bright extract -> blur -> tonemap to backbuffer
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

uint8_t SceneView::transparentViewId() const {
    return viewId_ + 2;
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

const std::vector<flecs::entity>& SceneView::transparentEntities() const {
    return transparentEntities_;
}

const std::vector<flecs::entity>& SceneView::opaqueEntities() const {
    return opaqueEntities_;
}

void SceneView::enablePostProcess(uint16_t width, uint16_t height) {
    postProcess_.init(width, height);
}

} // namespace fabric
