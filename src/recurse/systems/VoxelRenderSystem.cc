#include "recurse/systems/VoxelRenderSystem.hh"

#include "fabric/core/AppContext.hh"
#include "fabric/core/Camera.hh"
#include "fabric/core/Log.hh"
#include "fabric/core/SceneView.hh"
#include "fabric/core/SystemRegistry.hh"
#include "fabric/utils/Profiler.hh"
#include "recurse/systems/ChunkPipelineSystem.hh"
#include "recurse/systems/ParticleGameSystem.hh"
#include "recurse/systems/ShadowRenderSystem.hh"
#include "recurse/world/VoxelMesher.hh"

#include <SDL3/SDL.h>
#include <unordered_set>

namespace recurse::systems {

void VoxelRenderSystem::init(fabric::AppContext& ctx) {
    chunks_ = ctx.systemRegistry.get<ChunkPipelineSystem>();
    shadow_ = ctx.systemRegistry.get<ShadowRenderSystem>();
    particles_ = ctx.systemRegistry.get<ParticleGameSystem>();

    FABRIC_LOG_INFO("VoxelRenderSystem initialized");
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

    std::unordered_set<flecs::entity_t> visibleEntityIds;
    visibleEntityIds.reserve(sceneView->visibleEntities().size());
    for (const auto& entity : sceneView->visibleEntities()) {
        visibleEntityIds.insert(entity.id());
    }

    // Voxel chunk rendering (frustum-filtered via chunk entities)
    for (const auto& [coord, mesh] : chunks_->gpuMeshes()) {
        auto entIt = chunks_->chunkEntities().find(coord);
        if (entIt == chunks_->chunkEntities().end())
            continue;
        if (visibleEntityIds.find(entIt->second.id()) == visibleEntityIds.end())
            continue;
        voxelRenderer_.render(sceneView->geometryViewId(), mesh, coord.cx, coord.cy, coord.cz);
    }

    // Particle billboard rendering
    {
        int curPW, curPH;
        SDL_GetWindowSizeInPixels(window, &curPW, &curPH);
        particles_->renderParticles(camera->viewMatrix(), camera->projectionMatrix(), static_cast<uint16_t>(curPW),
                                    static_cast<uint16_t>(curPH));
    }
}

void VoxelRenderSystem::shutdown() {
    voxelRenderer_.shutdown();
    // Sky renderer is owned by SceneView but its GPU resources must be released
    // before bgfx shuts down. The scene renderer is the appropriate place since
    // it manages the SceneView render pipeline.
    // Note: ctx is not available in shutdown(). Wave 2 will wire this into
    // onShutdown or a ctx-aware shutdown path. Left as a reminder.
    FABRIC_LOG_INFO("VoxelRenderSystem shut down");
}

void VoxelRenderSystem::configureDependencies() {
    after<ChunkPipelineSystem>();
    after<ShadowRenderSystem>();
    after<ParticleGameSystem>();
}

} // namespace recurse::systems
