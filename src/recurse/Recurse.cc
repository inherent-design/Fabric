// Recurse.cc -- Main entry point for the Recurse game.
// Registers 15 application systems via FabricAppDesc and delegates to
// FabricApp::run(). All game logic lives in registered SystemBase subclasses.

// Engine includes
#include "fabric/core/AppContext.hh"
#include "fabric/core/AppModeManager.hh"
#include "fabric/core/ECS.hh"
#include "fabric/core/Event.hh"
#include "fabric/core/FabricApp.hh"
#include "fabric/core/FabricAppDesc.hh"
#include "fabric/core/InputManager.hh"
#include "fabric/core/InputRouter.hh"
#include "fabric/core/Log.hh"
#include "fabric/core/SceneView.hh"
#include "fabric/core/SystemPhase.hh"
#include "fabric/core/SystemRegistry.hh"
#include "fabric/core/Temporal.hh"

// Complete type headers required by registerSystem<T>() factory instantiation.
// System headers forward-declare types held in unique_ptr; the factory lambda
// needs their destructors visible at template instantiation time.
#include "recurse/gameplay/CharacterController.hh"
#include "recurse/gameplay/FlightController.hh"
#include "recurse/gameplay/VoxelInteraction.hh"
#include "recurse/persistence/SaveManager.hh"
// System includes
#include "recurse/systems/AIGameSystem.hh"
#include "recurse/systems/AudioGameSystem.hh"
#include "recurse/systems/CameraGameSystem.hh"
#include "recurse/systems/CharacterMovementSystem.hh"
#include "recurse/systems/ChunkPipelineSystem.hh"
#include "recurse/systems/DebugOverlaySystem.hh"
#include "recurse/systems/OITRenderSystem.hh"
#include "recurse/systems/ParticleGameSystem.hh"
#include "recurse/systems/PhysicsGameSystem.hh"
#include "recurse/systems/SaveGameSystem.hh"
#include "recurse/systems/ShadowRenderSystem.hh"
#include "recurse/systems/TerrainSystem.hh"
#include "recurse/systems/VoxelInteractionSystem.hh"
#include "recurse/systems/VoxelMeshingSystem.hh"
#include "recurse/systems/VoxelRenderSystem.hh"
#include "recurse/systems/VoxelSimulationSystem.hh"

#include <SDL3/SDL.h>

using fabric::SystemPhase;

fabric::FabricAppDesc buildRecurseDesc() {
    fabric::FabricAppDesc desc;
    desc.name = "Recurse";
    desc.configPath = "recurse.toml";
    desc.headless = false;

    // Register all systems unconditionally.
    // Dependencies declared in each system's configureDependencies().
    // Logging filters control verbosity at runtime.

    // FixedUpdate: simulation, physics, AI
    desc.registerSystem<recurse::systems::TerrainSystem>(SystemPhase::FixedUpdate);
    desc.registerSystem<recurse::systems::VoxelSimulationSystem>(SystemPhase::FixedUpdate);
    desc.registerSystem<recurse::systems::PhysicsGameSystem>(SystemPhase::FixedUpdate);
    desc.registerSystem<recurse::systems::CharacterMovementSystem>(SystemPhase::FixedUpdate);
    desc.registerSystem<recurse::systems::AIGameSystem>(SystemPhase::FixedUpdate);
    desc.registerSystem<recurse::systems::ParticleGameSystem>(SystemPhase::FixedUpdate);
    desc.registerSystem<recurse::systems::ChunkPipelineSystem>(SystemPhase::FixedUpdate);
    desc.registerSystem<recurse::systems::VoxelInteractionSystem>(SystemPhase::FixedUpdate);
    desc.registerSystem<recurse::systems::SaveGameSystem>(SystemPhase::FixedUpdate);

    // Update: per-frame logic
    desc.registerSystem<recurse::systems::AudioGameSystem>(SystemPhase::Update);
    desc.registerSystem<recurse::systems::CameraGameSystem>(SystemPhase::Update);

    // PreRender: meshing, shadows
    desc.registerSystem<recurse::systems::VoxelMeshingSystem>(SystemPhase::PreRender);
    desc.registerSystem<recurse::systems::ShadowRenderSystem>(SystemPhase::PreRender);

    // Render: submission
    desc.registerSystem<recurse::systems::VoxelRenderSystem>(SystemPhase::Render);
    desc.registerSystem<recurse::systems::OITRenderSystem>(SystemPhase::Render);
    desc.registerSystem<recurse::systems::DebugOverlaySystem>(SystemPhase::Render);

    // onInit: cross-cutting setup that spans multiple systems.
    // Key bindings, ECS core components, AppMode observer.
    // System-specific init handled by each system's init().
    desc.onInit = [](fabric::AppContext& ctx) {
        FABRIC_LOG_INFO("Recurse onInit: cross-cutting setup");

        auto& ecsWorld = ctx.world;
        auto& dispatcher = ctx.dispatcher;
        auto& timeline = ctx.timeline;

        // ECS core components
        ecsWorld.registerCoreComponents();
#ifdef FABRIC_ECS_INSPECTOR
        ecsWorld.enableInspector();
#endif

        // Key bindings (input action mapping)
        ctx.inputManager->bindKey("move_forward", SDLK_W);
        ctx.inputManager->bindKey("move_backward", SDLK_S);
        ctx.inputManager->bindKey("move_left", SDLK_A);
        ctx.inputManager->bindKey("move_right", SDLK_D);
        ctx.inputManager->bindKey("move_up", SDLK_SPACE);
        ctx.inputManager->bindKey("move_down", SDLK_LSHIFT);
        ctx.inputManager->bindKey("speed_boost", SDLK_LCTRL);

        ctx.inputManager->bindKey("time_pause", SDLK_P);
        ctx.inputManager->bindKey("time_faster", SDLK_EQUALS);
        ctx.inputManager->bindKey("time_slower", SDLK_MINUS);

        ctx.inputManager->bindKey("toggle_fly", SDLK_F);
        ctx.inputManager->bindKey("toggle_debug", SDLK_F3);
        ctx.inputManager->bindKey("toggle_wireframe", SDLK_F4);
        ctx.inputManager->bindKey("toggle_chunk_debug", SDLK_F12);
        ctx.inputManager->bindKey("toggle_camera", SDLK_V);
        ctx.inputManager->bindKey("toggle_collision_debug", SDLK_F10);
        ctx.inputManager->bindKey("toggle_bvh_debug", SDLK_F6);
        ctx.inputManager->bindKey("toggle_content_browser", SDLK_F7);
        ctx.inputManager->bindKey("toggle_bt_debug", SDLK_F11);
        ctx.inputManager->bindKey("cycle_bt_npc", SDLK_F8);

        // Timeline event listeners (cross-cutting; affect global simulation state)
        dispatcher.addEventListener("time_pause", [&timeline](fabric::Event&) {
            if (timeline.isPaused()) {
                timeline.resume();
                FABRIC_LOG_INFO("Timeline resumed");
            } else {
                timeline.pause();
                FABRIC_LOG_INFO("Timeline paused");
            }
        });

        dispatcher.addEventListener("time_faster", [&timeline](fabric::Event&) {
            double scale = timeline.getGlobalTimeScale() + 0.25;
            if (scale > 4.0)
                scale = 4.0;
            timeline.setGlobalTimeScale(scale);
            FABRIC_LOG_INFO("Time scale: {:.2f}", timeline.getGlobalTimeScale());
        });

        dispatcher.addEventListener("time_slower", [&timeline](fabric::Event&) {
            double scale = timeline.getGlobalTimeScale() - 0.25;
            if (scale < 0.25)
                scale = 0.25;
            timeline.setGlobalTimeScale(scale);
            FABRIC_LOG_INFO("Time scale: {:.2f}", timeline.getGlobalTimeScale());
        });

        // AppMode observer: mouse capture, simulation pause, and InputRouter sync
        auto* window = ctx.window;
        auto* inputRouter = ctx.inputRouter;
        ctx.appModeManager->addObserver([&timeline, window, inputRouter](fabric::AppMode, fabric::AppMode to) {
            const auto& modeFlags = fabric::AppModeManager::flags(to);
            SDL_SetWindowRelativeMouseMode(window, modeFlags.captureMouse);
            if (modeFlags.pauseSimulation) {
                timeline.pause();
            } else {
                timeline.resume();
            }
            // Sync InputRouter mode with AppMode routing flags
            if (modeFlags.routeToGame && !modeFlags.routeToUI) {
                inputRouter->setMode(fabric::InputMode::GameOnly);
            } else if (!modeFlags.routeToGame && modeFlags.routeToUI) {
                inputRouter->setMode(fabric::InputMode::UIOnly);
            } else {
                inputRouter->setMode(fabric::InputMode::GameAndUI);
            }
        });

        // Escape: toggle Game <-> Paused via AppMode (observer syncs everything)
        auto* appMode = ctx.appModeManager;
        ctx.inputRouter->setEscapeCallback([appMode]() {
            auto mode = appMode->current();
            if (mode == fabric::AppMode::Game) {
                appMode->transition(fabric::AppMode::Paused);
            } else if (mode == fabric::AppMode::Paused) {
                appMode->transition(fabric::AppMode::Game);
            }
        });

        // Apply initial mode flags (no transition fires for the startup state)
        SDL_SetWindowRelativeMouseMode(ctx.window,
                                       fabric::AppModeManager::flags(ctx.appModeManager->current()).captureMouse);

        FABRIC_LOG_INFO("Recurse onInit complete");
    };

    // onResize: delegate to OIT compositor and PostProcess (resolution-dependent GPU resources)
    desc.onResize = [](fabric::AppContext& ctx, uint32_t width, uint32_t height) {
        auto* oit = ctx.systemRegistry.get<recurse::systems::OITRenderSystem>();
        if (oit)
            oit->resize(static_cast<uint16_t>(width), static_cast<uint16_t>(height));

        if (ctx.sceneView)
            ctx.sceneView->postProcess().resize(static_cast<uint16_t>(width), static_cast<uint16_t>(height));
    };

    // onShutdown: handle resources that need AppContext access beyond system shutdown.
    // skyRenderer lives on SceneView (engine-owned); systems cannot reach it.
    desc.onShutdown = [](fabric::AppContext& ctx) {
        ctx.sceneView->skyRenderer().shutdown();
    };

    return desc;
}

int main(int argc, char* argv[]) {
    fabric::log::init();
    int result = fabric::FabricApp::run(argc, argv, buildRecurseDesc());
    fabric::log::shutdown();
    return result;
}
