#pragma once

#include "fabric/core/Camera.hh"
#include "fabric/core/Rendering.hh"
#include "fabric/core/Spatial.hh"
#include <cstdint>
#include <vector>

namespace fabric {

// Owns a bgfx view ID, a Camera reference, and a scene root.
// Orchestrates the per-frame render pipeline:
// update camera -> extract frustum -> cull -> build render list -> submit.
class SceneView {
public:
    SceneView(uint8_t viewId, Camera& camera, SceneNode& root);

    // Set clear color and flags for this view
    void setClearColor(uint32_t rgba);

    // Execute the render pipeline for one frame
    void render();

    uint8_t viewId() const;
    Camera& camera();
    const std::vector<SceneNode*>& visibleNodes() const;

private:
    uint8_t viewId_;
    Camera& camera_;
    SceneNode& root_;
    RenderList renderList_;
    std::vector<SceneNode*> visibleNodes_;
    uint32_t clearColor_ = 0x303030ff;
};

} // namespace fabric
