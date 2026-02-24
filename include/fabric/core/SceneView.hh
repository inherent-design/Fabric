#pragma once

#include "fabric/core/Camera.hh"
#include "fabric/core/Rendering.hh"
#include <cstdint>
#include <flecs.h>
#include <vector>

namespace fabric {

// Owns a bgfx view ID, a Camera reference, and a Flecs world reference.
// Orchestrates the per-frame render pipeline:
// update camera -> extract frustum -> cull -> build render list -> submit.
class SceneView {
  public:
    SceneView(uint8_t viewId, Camera& camera, flecs::world& world);

    // Set clear color and flags for this view
    void setClearColor(uint32_t rgba);

    // Execute the render pipeline for one frame
    void render();

    uint8_t viewId() const;
    Camera& camera();
    const std::vector<flecs::entity>& visibleEntities() const;

  private:
    uint8_t viewId_;
    Camera& camera_;
    flecs::world& world_;
    RenderList renderList_;
    std::vector<flecs::entity> visibleEntities_;
    uint32_t clearColor_ = 0x303030ff;
};

} // namespace fabric
