#include "fabric/core/SceneView.hh"
#include <bgfx/bgfx.h>

namespace fabric {

SceneView::SceneView(uint8_t viewId, Camera& camera, SceneNode& root)
    : viewId_(viewId), camera_(camera), root_(root) {}

void SceneView::setClearColor(uint32_t rgba) {
    clearColor_ = rgba;
}

void SceneView::render() {
    // 1. Get VP matrix from camera
    float vp[16];
    camera_.getViewProjection(vp);

    // 2. Cull scene against frustum
    visibleNodes_ = FrustumCuller::cull(vp, root_);

    // 3. Build render list from visible nodes
    renderList_.clear();
    for (SceneNode* node : visibleNodes_) {
        DrawCall dc;
        dc.viewId = viewId_;
        auto globalMatrix = node->getGlobalTransform().getMatrix();
        for (int i = 0; i < 16; ++i) {
            dc.transform[static_cast<size_t>(i)] = globalMatrix.elements[static_cast<size_t>(i)];
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

const std::vector<SceneNode*>& SceneView::visibleNodes() const {
    return visibleNodes_;
}

} // namespace fabric
