#include "recurse/systems/VoxelRenderSystem.hh"

#include "fabric/core/AppContext.hh"
#include "fabric/core/Spatial.hh"
#include "fabric/core/SystemRegistry.hh"
#include "fabric/log/Log.hh"
#include "fabric/render/Camera.hh"
#include "fabric/render/Geometry.hh"
#include "fabric/render/SceneView.hh"
#include "fabric/utils/Profiler.hh"
#include "recurse/systems/ChunkPipelineSystem.hh"
#include "recurse/systems/ParticleGameSystem.hh"
#include "recurse/systems/ShadowRenderSystem.hh"
#include "recurse/systems/VoxelMeshingSystem.hh"

#include <SDL3/SDL.h>

#include <bgfx/bgfx.h>

#include <algorithm>
#include <array>
#include <vector>

namespace recurse::systems {

void VoxelRenderSystem::doInit(fabric::AppContext& ctx) {
    chunks_ = ctx.systemRegistry.get<ChunkPipelineSystem>();
    meshSystem_ = ctx.systemRegistry.get<VoxelMeshingSystem>();
    shadow_ = ctx.systemRegistry.get<ShadowRenderSystem>();
    particles_ = ctx.systemRegistry.get<ParticleGameSystem>();

    FABRIC_LOG_INFO("VoxelRenderSystem initialized (mesh source: VoxelMeshingSystem)");
}

void VoxelRenderSystem::render(fabric::AppContext& ctx) {
    FABRIC_ZONE_SCOPED_N("render_submit");
    renderedChunkCount_ = 0;

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

        // Set actual camera world position for view-dependent lighting effects
        // (rim, specular). Chunks are transformed camera-relative, but shader
        // world positions are also camera-relative, so we need (0,0,0) here.
        // However, the shader needs to compute view direction correctly.
        // Since v_worldPos is camera-relative, viewPos should be (0,0,0).
        // The lighting direction is still in world space and correct.
        voxelRenderer_.setViewPosition(0.0, 0.0, 0.0);

        // Extract frustum for chunk-level culling
        std::array<float, 16> vpMatrix;
        camera->getViewProjection(vpMatrix.data());
        fabric::Frustum frustum;
        frustum.extractFromVP(vpMatrix.data());

        // Pre-compute chunk AABB size
        constexpr float chunkSize = static_cast<float>(recurse::K_CHUNK_SIZE);

        for (const auto& [coord, gpuMesh] : meshSystem_->gpuMeshes()) {
            if (!gpuMesh.valid || gpuMesh.vertexCount == 0 || gpuMesh.indexCount == 0)
                continue;

            // Build chunk AABB for frustum test
            fabric::Vec3f chunkMin(static_cast<float>(coord.x * recurse::K_CHUNK_SIZE),
                                   static_cast<float>(coord.y * recurse::K_CHUNK_SIZE),
                                   static_cast<float>(coord.z * recurse::K_CHUNK_SIZE));
            fabric::Vec3f chunkMax(chunkMin.x + chunkSize, chunkMin.y + chunkSize, chunkMin.z + chunkSize);
            fabric::AABB chunkAABB(chunkMin, chunkMax);

            // Skip chunks outside the view frustum
            if (frustum.testAABB(chunkAABB) == fabric::CullResult::Outside)
                continue;

            const auto worldOrigin =
                fabric::Vector3<double, fabric::Space::World>(static_cast<double>(coord.x * recurse::K_CHUNK_SIZE),
                                                              static_cast<double>(coord.y * recurse::K_CHUNK_SIZE),
                                                              static_cast<double>(coord.z * recurse::K_CHUNK_SIZE));
            const auto relOrigin = camera->cameraRelative(worldOrigin);

            renderBatch.push_back(recurse::ChunkRenderInfo{
                .mesh = &gpuMesh.mesh,
                .offsetX = relOrigin.x,
                .offsetY = relOrigin.y,
                .offsetZ = relOrigin.z,
                .sortKey = static_cast<uint64_t>(static_cast<uint16_t>(coord.x + 32768)) << 32 |
                           static_cast<uint64_t>(static_cast<uint16_t>(coord.y + 32768)) << 16 |
                           static_cast<uint64_t>(static_cast<uint16_t>(coord.z + 32768)),
            });
        }

        // Sort render batch by chunk coordinate for deterministic palette grouping.
        // This prevents color flickering caused by non-deterministic unordered_map iteration.
        std::sort(
            renderBatch.begin(), renderBatch.end(),
            [](const recurse::ChunkRenderInfo& a, const recurse::ChunkRenderInfo& b) { return a.sortKey < b.sortKey; });

        if (!renderBatch.empty()) {
            const bool wireframeEnabled = voxelRenderer_.isWireframeEnabled();
            renderedChunkCount_ = static_cast<int>(renderBatch.size());
            if (wireframeEnabled) {
                bgfx::setDebug(BGFX_DEBUG_WIREFRAME);
            }
            voxelRenderer_.renderBatch(sceneView->geometryViewId(), renderBatch.data(),
                                       static_cast<uint32_t>(renderBatch.size()));
            if (wireframeEnabled) {
                bgfx::setDebug(BGFX_DEBUG_NONE);
            }
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

void VoxelRenderSystem::doShutdown() {
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
