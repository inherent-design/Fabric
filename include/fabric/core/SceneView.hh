#pragma once

#include "fabric/core/Camera.hh"
#include "fabric/core/PaniniPass.hh"
#include "fabric/core/PostProcess.hh"
#include "fabric/core/Rendering.hh"
#include "fabric/core/SkyRenderer.hh"
#include <cstdint>
#include <flecs.h>
#include <vector>

namespace fabric {

// Camera projection mode for post-processing.
// TODO(ProjectionMode): Add Fisheye (spherical distortion) and Isometric (camera-level, not post-process)
// TODO(ProjectionMode): Consider VR Stereo (dual-view, separate pass) for future HMD support
// TODO(Console): Make this configurable via console commands for runtime exploration
enum class ProjectionMode {
    Perspective, // Default rectilinear projection (no correction)
    Panini,      // Cylindrical projection for high-FOV (120-150°)
    Equirect,    // 360° panoramic (TODO: implement shader)
    // TODO: Fisheye, Isometric, VR_Stereo
};

// View ID layout:
//   viewId_     = sky dome (clear color+depth, fullscreen triangle)
//   viewId_+1   = opaque geometry pass (depth write on)
//   viewId_+2   = transparent geometry pass (depth write off, alpha blend, back-to-front)
//   199         = projection correction (Panini/Equirect/Fisheye)
//   200..205    = post-process chain (bright, blur x4, tonemap)
//   240..243    = shadows
class SceneView {
  public:
    SceneView(uint8_t viewId, Camera& camera, flecs::world& world);

    // Set clear color and flags for this view
    void setClearColor(uint32_t rgba);

    // Execute the render pipeline for one frame:
    //   1. Cull visible entities via BVH
    //   2. Partition into opaque and transparent lists
    //   3. Sort transparent list back-to-front
    //   4. Submit opaque to geometryViewId(), transparent to transparentViewId()
    void render();

    uint8_t viewId() const;

    // View ID for opaque geometry submission (viewId_ + 1).
    // External renderers (VoxelRenderer, etc.) should submit to this view.
    uint8_t geometryViewId() const;

    // View ID for transparent geometry submission (viewId_ + 2).
    // Transparent entities are sorted back-to-front and rendered with alpha blend.
    uint8_t transparentViewId() const;

    Camera& camera();
    SkyRenderer& skyRenderer();
    PostProcess& postProcess();
    const std::vector<flecs::entity>& visibleEntities() const;
    const std::vector<flecs::entity>& transparentEntities() const;
    const std::vector<flecs::entity>& opaqueEntities() const;

    // Set the viewport dimensions for all sub-views (sky, geometry, transparent).
    // Must be called after bgfx::init() and again on window resize.
    void setViewport(uint16_t width, uint16_t height);

    // Enable HDR post-processing. Must be called after bgfx::init().
    void enablePostProcess(uint16_t width, uint16_t height);

    // Current viewport dimensions.
    uint16_t viewWidth() const { return viewWidth_; }
    uint16_t viewHeight() const { return viewHeight_; }

    // Camera projection mode
    ProjectionMode projectionMode() const;
    void cycleProjectionMode(); // Alt+C: Panini -> Equirect -> Panini

  private:
    uint8_t viewId_;
    Camera& camera_;
    flecs::world& world_;
    FrustumCuller frustumCuller_;
    SkyRenderer skyRenderer_;
    PostProcess postProcess_;
    RenderList renderList_;
    RenderList transparentRenderList_;
    std::vector<flecs::entity> visibleEntities_;
    std::vector<flecs::entity> opaqueEntities_;
    std::vector<flecs::entity> transparentEntities_;
    uint32_t clearColor_ = 0x303030ff;
    uint16_t viewWidth_ = 0;
    uint16_t viewHeight_ = 0;

    // Projection correction pass (Panini/Equirect)
    PaniniPass paniniPass_;
    ProjectionMode projectionMode_ = ProjectionMode::Panini; // Default to Panini

    // Build a DrawCall for an entity and add it to the given render list with the given view ID
    void buildDrawCall(flecs::entity entity, uint8_t viewId, RenderList& list);
};

} // namespace fabric
