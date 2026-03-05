#include "recurse/systems/VoxelRenderSystem.hh"

#include "fabric/core/AppContext.hh"
#include "fabric/core/Camera.hh"
#include "fabric/core/Log.hh"
#include "fabric/core/SceneView.hh"
#include "fabric/core/Spatial.hh"
#include "fabric/core/SystemRegistry.hh"
#include "fabric/utils/Profiler.hh"
#include "recurse/systems/ChunkPipelineSystem.hh"
#include "recurse/systems/ParticleGameSystem.hh"
#include "recurse/systems/ShadowRenderSystem.hh"
#include "recurse/systems/VoxelMeshingSystem.hh"

#include <SDL3/SDL.h>

#include <vector>

namespace recurse::systems {

void VoxelRenderSystem::init(fabric::AppContext& ctx) {
    chunks_ = ctx.systemRegistry.get<ChunkPipelineSystem>();
    meshSystem_ = ctx.systemRegistry.get<VoxelMeshingSystem>();
    shadow_ = ctx.systemRegistry.get<ShadowRenderSystem>();
    particles_ = ctx.systemRegistry.get<ParticleGameSystem>();

    FABRIC_LOG_INFO("VoxelRenderSystem initialized (mesh source: VoxelMeshingSystem)");
}

void VoxelRenderSystem::render(fabric::AppContext& ctx) {
    FABRIC_ZONE_SCOPED_N("render_submit");

    auto* camera = ctx.camera;
    auto* sceneView = ctx.sceneView;
    auto* window = ctx.window;

    // Set light direction on the voxel shader
    auto& lightDir = shadow_->lightDirection();
    voxelRenderer_.setLightDirection(fabric::Vector3<float, fabric::Space::World>(lightDir.x, lightDir.y, lightDir.z));

    // ECS entity rendering (cull, build render list, submit)
    sceneView->render();

    // Voxel chunk mesh rendering from VoxelMeshingSystem
    if (meshSystem_) {
        FABRIC_ZONE_SCOPED_N("voxel_chunk_render");

        std::vector<recurse::ChunkRenderInfo> renderBatch;
        renderBatch.reserve(meshSystem_->gpuMeshes().size());

        // Camera-relative render path: chunk transforms are submitted relative
        // to camera, so shader-space camera position is origin.
        voxelRenderer_.setViewPosition(0.0, 0.0, 0.0);

        for (const auto& [coord, gpuMesh] : meshSystem_->gpuMeshes()) {
            if (!gpuMesh.valid || gpuMesh.vertexCount == 0 || gpuMesh.indexCount == 0)
                continue;

            const auto worldOrigin = fabric::Vector3<double, fabric::Space::World>(
                static_cast<double>(coord.x * recurse::kChunkSize), static_cast<double>(coord.y * recurse::kChunkSize),
                static_cast<double>(coord.z * recurse::kChunkSize));
            const auto relOrigin = camera->cameraRelative(worldOrigin);

            renderBatch.push_back(recurse::ChunkRenderInfo{
                .mesh = &gpuMesh.mesh,
                .offsetX = relOrigin.x,
                .offsetY = relOrigin.y,
                .offsetZ = relOrigin.z,
            });
        }

        if (!renderBatch.empty()) {
            voxelRenderer_.renderBatch(sceneView->geometryViewId(), renderBatch.data(),
                                       static_cast<uint32_t>(renderBatch.size()));
        }
    }

    // Particle billboard rendering (optional - may be disabled via feature flag)
    if (particles_) {
        int curPW, curPH;
        SDL_GetWindowSizeInPixels(window, &curPW, &curPH);
        particles_->renderParticles(camera->viewMatrix(), camera->projectionMatrix(), static_cast<uint16_t>(curPW),
                                    static_cast<uint16_t>(curPH));
    }
}

void VoxelRenderSystem::shutdown() {
    voxelRenderer_.shutdown();
    FABRIC_LOG_INFO("VoxelRenderSystem shut down");
}

void VoxelRenderSystem::configureDependencies() {
    after<ChunkPipelineSystem>();
    after<VoxelMeshingSystem>();
    after<ShadowRenderSystem>();
    after<ParticleGameSystem>();
}

} // namespace recurse::systems
