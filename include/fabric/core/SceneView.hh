#pragma once

#include "fabric/core/Camera.hh"
#include "fabric/core/PostProcess.hh"
#include "fabric/core/Rendering.hh"
#include "fabric/core/SkyRenderer.hh"
#include <cstdint>
#include <flecs.h>
#include <vector>

namespace fabric {

// Owns a bgfx view ID, a Camera reference, and a Flecs world reference.
// Orchestrates the per-frame render pipeline:
// update camera -> extract frustum -> cull -> build render list -> submit.
//
// View layout:
//   viewId_     - sky dome (clear color+depth, fullscreen triangle)
//   viewId_ + 1 - geometry (no clear, depth test inherits cleared buffer)
//   200..205    - post-process chain (bright, blur x4, tonemap)
// Shadow views at 240-243 are unaffected.
class SceneView {
  public:
    SceneView(uint8_t viewId, Camera& camera, flecs::world& world);

    // Set clear color and flags for this view
    void setClearColor(uint32_t rgba);

    // Execute the render pipeline for one frame
    void render();

    uint8_t viewId() const;

    // View ID for opaque geometry submission (viewId_ + 1).
    // External renderers (VoxelRenderer, etc.) should submit to this view.
    uint8_t geometryViewId() const;

    Camera& camera();
    SkyRenderer& skyRenderer();
    PostProcess& postProcess();
    const std::vector<flecs::entity>& visibleEntities() const;

    // Enable HDR post-processing. Must be called after bgfx::init().
    void enablePostProcess(uint16_t width, uint16_t height);

  private:
    uint8_t viewId_;
    Camera& camera_;
    flecs::world& world_;
    SkyRenderer skyRenderer_;
    PostProcess postProcess_;
    RenderList renderList_;
    std::vector<flecs::entity> visibleEntities_;
    uint32_t clearColor_ = 0x303030ff;
};

} // namespace fabric
