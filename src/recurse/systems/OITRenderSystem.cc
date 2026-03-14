#include "recurse/systems/OITRenderSystem.hh"
#include "recurse/render/OITCompositor.hh"

#include "fabric/core/AppContext.hh"
#include "fabric/log/Log.hh"
#include "fabric/render/Camera.hh"
#include "fabric/render/Rendering.hh"
#include "fabric/render/SceneView.hh"
#include "recurse/systems/VoxelRenderSystem.hh"

#include <SDL3/SDL.h>

namespace recurse::systems {

void OITRenderSystem::doInit(fabric::AppContext& ctx) {
    int pw, ph;
    SDL_GetWindowSizeInPixels(ctx.window, &pw, &ph);
    oitCompositor_.init(static_cast<uint16_t>(pw), static_cast<uint16_t>(ph));

    FABRIC_LOG_INFO("OITRenderSystem initialized");
}

void OITRenderSystem::render(fabric::AppContext& ctx) {
    if (!oitCompositor_.isValid())
        return;

    auto* sceneView = ctx.sceneView;
    if (sceneView->transparentEntities().empty())
        return;

    auto* camera = ctx.camera;
    auto* window = ctx.window;

    int oitPW, oitPH;
    SDL_GetWindowSizeInPixels(window, &oitPW, &oitPH);

    oitCompositor_.beginAccumulation(fabric::K_OIT_ACCUM_VIEW_ID, camera->viewMatrix(), camera->projectionMatrix(),
                                     static_cast<uint16_t>(oitPW), static_cast<uint16_t>(oitPH));
    oitCompositor_.composite(fabric::K_OIT_COMPOSITE_VIEW_ID, static_cast<uint16_t>(oitPW),
                             static_cast<uint16_t>(oitPH));
}

void OITRenderSystem::doShutdown() {
    oitCompositor_.shutdown();
    FABRIC_LOG_INFO("OITRenderSystem shut down");
}

void OITRenderSystem::configureDependencies() {
    // Composite after opaque geometry
    after<VoxelRenderSystem>();
}

void OITRenderSystem::resize(uint16_t width, uint16_t height) {
    if (oitCompositor_.isValid()) {
        oitCompositor_.shutdown();
    }
    oitCompositor_.init(width, height);
}

} // namespace recurse::systems
