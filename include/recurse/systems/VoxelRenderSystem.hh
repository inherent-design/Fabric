#pragma once

#include "fabric/core/SystemBase.hh"
#include "recurse/render/VoxelRenderer.hh"

namespace recurse::systems {

class ChunkPipelineSystem;
class VoxelMeshingSystem;
class ShadowRenderSystem;
class ParticleGameSystem;

/// Submits opaque voxel chunk geometry and particle billboards to bgfx.
/// Frustum-filters chunks using the ECS visibility set from SceneView::render().
class VoxelRenderSystem : public fabric::System<VoxelRenderSystem> {
  public:
    void init(fabric::AppContext& ctx) override;
    void render(fabric::AppContext& ctx) override;
    void shutdown() override;
    void configureDependencies() override;

    recurse::VoxelRenderer& voxelRenderer() { return voxelRenderer_; }

  private:
    recurse::VoxelRenderer voxelRenderer_;

    ChunkPipelineSystem* chunks_ = nullptr;
    VoxelMeshingSystem* meshSystem_ = nullptr;
    ShadowRenderSystem* shadow_ = nullptr;
    ParticleGameSystem* particles_ = nullptr;
};

} // namespace recurse::systems
