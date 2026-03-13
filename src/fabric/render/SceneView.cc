#include "fabric/render/SceneView.hh"
#include "fabric/core/ECS.hh"
#include "fabric/core/Log.hh"
#include "fabric/core/Spatial.hh"
#include "fabric/utils/Profiler.hh"
#include <bgfx/bgfx.h>

namespace fabric {

namespace {
constexpr uint8_t K_PROJECTION_VIEW_ID = 210;
}

SceneView::SceneView(uint8_t viewId, Camera& camera, flecs::world& world)
    : viewId_(viewId), camera_(camera), world_(world), projectionMode_(ProjectionMode::Panini) {}

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
        const auto camPosD = camera_.worldPositionD();
        Vec3f camPos(static_cast<float>(camPosD.x), static_cast<float>(camPosD.y), static_cast<float>(camPosD.z));
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

    bgfx::setViewRect(viewId_, 0, 0, viewWidth_, viewHeight_);
    bgfx::setViewTransform(viewId_, camera_.viewMatrix(), camera_.projectionMatrix());
    bgfx::setViewClear(viewId_, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, clearColor_, 1.0f, 0);
    skyRenderer_.init();
    skyRenderer_.render(viewId_);
    bgfx::touch(viewId_);

    // 7. Geometry view (viewId_ + 1): no clear, inherits depth from sky view
    bgfx::setViewRect(geoView, 0, 0, viewWidth_, viewHeight_);
    bgfx::setViewTransform(geoView, camera_.viewMatrix(), camera_.projectionMatrix());
    bgfx::setViewClear(geoView, BGFX_CLEAR_NONE);
    bgfx::touch(geoView);

    // 8. Transparent view (viewId_ + 2): no depth write, alpha blend, back-to-front
    if (!transparentEntities_.empty()) {
        if (postProcess_.isValid()) {
            bgfx::setViewFrameBuffer(transparentViewId(), postProcess_.hdrFramebuffer());
        }
        bgfx::setViewRect(transparentViewId(), 0, 0, viewWidth_, viewHeight_);
        bgfx::setViewTransform(transparentViewId(), camera_.viewMatrix(), camera_.projectionMatrix());
        // No clear on transparent pass: it composites over the geometry pass
        bgfx::setViewClear(transparentViewId(), BGFX_CLEAR_NONE);
        bgfx::touch(transparentViewId());
    }

    // 9. Post-process: bright extract -> blur -> tonemap
    if (postProcess_.isValid()) {
        bool applyProjection =
            paniniPass_.isValid() && projectionFb_.isValid() && projectionMode_ == ProjectionMode::Panini;

        if (applyProjection) {
            postProcess_.setOutputTarget(projectionFb_.get());
        } else {
            bgfx::FrameBufferHandle backbuffer = BGFX_INVALID_HANDLE;
            postProcess_.setOutputTarget(backbuffer);
        }
        postProcess_.render(200);

        // 10. Panini projection (reads tonemapped output, writes to backbuffer)
        if (applyProjection) {
            auto projTex = bgfx::getTexture(projectionFb_.get(), 0);
            paniniPass_.setEnabled(true);
            paniniPass_.execute(projTex, K_PROJECTION_VIEW_ID);
        }
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

void SceneView::setViewport(uint16_t width, uint16_t height) {
    viewWidth_ = width;
    viewHeight_ = height;
    if (postProcess_.isValid()) {
        postProcess_.resize(width, height);
        paniniPass_.resize(width, height);
        projectionFb_.reset();
        createProjectionFb(width, height);
    }
    updateCameraFOV();
}

void SceneView::enablePostProcess(uint16_t width, uint16_t height) {
    postProcess_.init(width, height);
    paniniPass_.init(width, height);
    createProjectionFb(width, height);
}

void SceneView::cycleProjectionMode() {
    switch (projectionMode_) {
        case ProjectionMode::Perspective:
            projectionMode_ = ProjectionMode::Panini;
            FABRIC_LOG_INFO("Projection mode: Panini (FOV {})", paniniPass_.fovDeg());
            break;
        case ProjectionMode::Panini:
            projectionMode_ = ProjectionMode::Perspective;
            FABRIC_LOG_INFO("Projection mode: Perspective (FOV {})", perspectiveFovDeg_);
            break;
        // Equirect not yet implemented; skip in cycle, re-add when shader exists
        case ProjectionMode::Equirect:
            projectionMode_ = ProjectionMode::Perspective;
            FABRIC_LOG_INFO("Projection mode: Perspective (FOV {})", perspectiveFovDeg_);
            break;
    }
    updateCameraFOV();
}

ProjectionMode SceneView::projectionMode() const {
    return projectionMode_;
}

PaniniPass& SceneView::paniniPass() {
    return paniniPass_;
}

void SceneView::createProjectionFb(uint16_t width, uint16_t height) {
    if (width == 0 || height == 0)
        return;
    projectionFb_.reset(bgfx::createFrameBuffer(width, height, bgfx::TextureFormat::RGBA8,
                                                BGFX_TEXTURE_RT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP));
}

void SceneView::setPerspectiveFOV(float degrees) {
    perspectiveFovDeg_ = degrees;
    updateCameraFOV();
}

void SceneView::updateCameraFOV() {
    if (viewWidth_ == 0 || viewHeight_ == 0)
        return;
    float fov = perspectiveFovDeg_;
    if (projectionMode_ == ProjectionMode::Panini && paniniPass_.isValid()) {
        fov = paniniPass_.fovDeg();
    }
    float aspect = static_cast<float>(viewWidth_) / static_cast<float>(viewHeight_);
    camera_.setPerspective(fov, aspect, camera_.nearPlane(), camera_.farPlane(), bgfx::getCaps()->homogeneousDepth);
}

} // namespace fabric
