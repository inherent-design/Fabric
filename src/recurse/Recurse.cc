// Recurse.cc -- Main entry point for the Recurse game.
// Registers 14 application systems via FabricAppDesc and delegates to
// FabricApp::run(). All game logic lives in registered SystemBase subclasses.

// Engine includes
#include "fabric/core/AppContext.hh"
#include "fabric/core/AppModeManager.hh"
#include "fabric/core/Event.hh"
#include "fabric/core/SystemPhase.hh"
#include "fabric/core/SystemRegistry.hh"
#include "fabric/core/Temporal.hh"
#include "fabric/ecs/ECS.hh"
#include "fabric/input/InputManager.hh"
#include "fabric/input/InputRouter.hh"
#include "fabric/log/Log.hh"
#include "fabric/platform/FabricApp.hh"
#include "fabric/platform/FabricAppDesc.hh"
#include "fabric/render/SceneView.hh"

// Complete type headers required by registerSystem<T>() factory instantiation.
// System headers forward-declare types held in unique_ptr; the factory lambda
// needs their destructors visible at template instantiation time.
#include "fabric/platform/PlatformInfo.hh"
#include "recurse/character/CharacterController.hh"
#include "recurse/character/FlightController.hh"
#include "recurse/character/VoxelInteraction.hh"
#include "recurse/persistence/WorldRegistry.hh"
// System includes
#include "recurse/systems/AIGameSystem.hh"
#include "recurse/systems/AudioGameSystem.hh"
#include "recurse/systems/CameraGameSystem.hh"
#include "recurse/systems/CharacterMovementSystem.hh"
#include "recurse/systems/ChunkPipelineSystem.hh"
#include "recurse/systems/DebugOverlaySystem.hh"
#include "recurse/systems/LODSystem.hh"
#include "recurse/systems/MainMenuSystem.hh"
#include "recurse/systems/OITRenderSystem.hh"
#include "recurse/systems/ParticleGameSystem.hh"
#include "recurse/systems/PhysicsGameSystem.hh"
#include "recurse/systems/ShadowRenderSystem.hh"
#include "recurse/systems/TerrainSystem.hh"
#include "recurse/systems/VoxelInteractionSystem.hh"
#include "recurse/systems/VoxelMeshingSystem.hh"
#include "recurse/systems/VoxelRenderSystem.hh"
#include "recurse/systems/VoxelSimulationSystem.hh"

#include "recurse/input/ActionIds.hh"
#include "recurse/input/DebugToggleTable.hh"

#include <SDL3/SDL.h>

using fabric::SystemPhase;
using namespace recurse::input;

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

    // Update: per-frame logic
    desc.registerSystem<recurse::systems::MainMenuSystem>(SystemPhase::Update);
    desc.registerSystem<recurse::systems::AudioGameSystem>(SystemPhase::Update);
    desc.registerSystem<recurse::systems::CameraGameSystem>(SystemPhase::Update);

    // PreRender: meshing, shadows, LOD
    desc.registerSystem<recurse::systems::VoxelMeshingSystem>(SystemPhase::PreRender);
    desc.registerSystem<recurse::systems::LODSystem>(SystemPhase::PreRender);
    desc.registerSystem<recurse::systems::ShadowRenderSystem>(SystemPhase::PreRender);

    // Render: submission
    desc.registerSystem<recurse::systems::VoxelRenderSystem>(SystemPhase::Render);
    desc.registerSystem<recurse::systems::OITRenderSystem>(SystemPhase::Render);
    desc.registerSystem<recurse::systems::DebugOverlaySystem>(SystemPhase::Render);

    // onInit: cross-cutting setup that spans multiple systems.
    // Key bindings, ECS core components, AppMode observer.
    // System-specific init handled by each system's init().
    // WorldRegistry persists for the app lifetime; shared_ptr captured by onInit lambda.
    auto worldRegistry = std::make_shared<recurse::WorldRegistry>("");

    desc.onInit = [worldRegistry](fabric::AppContext& ctx) {
        FABRIC_LOG_INFO("Recurse onInit: cross-cutting setup");

        auto& ecsWorld = ctx.world;
        auto& dispatcher = ctx.dispatcher;
        auto& timeline = ctx.timeline;

        // Initialize WorldRegistry with platform data directory
        if (ctx.platformInfo) {
            std::string worldsDir = ctx.platformInfo->dirs.dataDir + "/worlds";
            *worldRegistry = recurse::WorldRegistry(worldsDir);
            FABRIC_LOG_INFO("WorldRegistry initialized: {}", worldsDir);
        }

        // Wire WorldRegistry to MainMenuSystem
        auto* mainMenu = ctx.systemRegistry.get<recurse::systems::MainMenuSystem>();
        if (mainMenu) {
            mainMenu->setWorldRegistry(worldRegistry.get());
        }

        // ECS core components
        ecsWorld.registerCoreComponents();
#ifdef FABRIC_ECS_INSPECTOR
        ecsWorld.enableInspector();
#endif

        // Key bindings (input action mapping)
        ctx.inputManager->bindKey(K_ACTION_MOVE_FORWARD, SDLK_W);
        ctx.inputManager->bindKey(K_ACTION_MOVE_BACKWARD, SDLK_S);
        ctx.inputManager->bindKey(K_ACTION_MOVE_LEFT, SDLK_A);
        ctx.inputManager->bindKey(K_ACTION_MOVE_RIGHT, SDLK_D);
        ctx.inputManager->bindKey(K_ACTION_MOVE_UP, SDLK_SPACE);
        ctx.inputManager->bindKey(K_ACTION_MOVE_DOWN, SDLK_LSHIFT);
        ctx.inputManager->bindKey(K_ACTION_SPEED_BOOST, SDLK_LCTRL);

        ctx.inputManager->bindKey(K_ACTION_TIME_PAUSE, SDLK_P);
        ctx.inputManager->bindKey(K_ACTION_TIME_FASTER, SDLK_EQUALS);
        ctx.inputManager->bindKey(K_ACTION_TIME_SLOWER, SDLK_MINUS);

        ctx.inputManager->bindKey(K_ACTION_TOGGLE_FLY, SDLK_F);
        ctx.inputManager->bindKey(K_ACTION_TOGGLE_CAMERA, SDLK_V);

        // Debug toggle bindings from table (single source of truth)
        for (const auto& toggle : K_DEBUG_TOGGLES) {
            ctx.inputManager->bindKey(toggle.actionId, toggle.defaultKey);
        }

        ctx.inputManager->bindKey(K_ACTION_TOGGLE_CONTENT_BROWSER, SDLK_F7);
        ctx.inputManager->bindKey(K_ACTION_TOGGLE_BT_DEBUG, SDLK_F11);
        ctx.inputManager->bindKey(K_ACTION_CYCLE_BT_NPC, SDLK_F8);

        // Timeline event listeners (cross-cutting; affect global simulation state)
        dispatcher.addEventListener(K_ACTION_TIME_PAUSE, [&timeline](fabric::Event&) {
            if (timeline.isPaused()) {
                timeline.resume();
                FABRIC_LOG_INFO("Timeline resumed");
            } else {
                timeline.pause();
                FABRIC_LOG_INFO("Timeline paused");
            }
        });

        dispatcher.addEventListener(K_ACTION_TIME_FASTER, [&timeline](fabric::Event&) {
            double scale = timeline.getGlobalTimeScale() + 0.25;
            if (scale > 4.0)
                scale = 4.0;
            timeline.setGlobalTimeScale(scale);
            FABRIC_LOG_INFO("Time scale: {:.2f}", timeline.getGlobalTimeScale());
        });

        dispatcher.addEventListener(K_ACTION_TIME_SLOWER, [&timeline](fabric::Event&) {
            double scale = timeline.getGlobalTimeScale() - 0.25;
            if (scale < 0.25)
                scale = 0.25;
            timeline.setGlobalTimeScale(scale);
            FABRIC_LOG_INFO("Time scale: {:.2f}", timeline.getGlobalTimeScale());
        });

        // AppMode observer: mouse capture, simulation pause, and InputRouter sync
        auto* window = ctx.window;
        auto* inputRouter = ctx.inputRouter;
        ctx.appModeManager->addObserver([&timeline, window, inputRouter](fabric::AppMode from, fabric::AppMode to) {
            FABRIC_LOG_INFO("AppMode observer: {} -> {}, setting InputRouter", fabric::appModeToString(from),
                            fabric::appModeToString(to));
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
                FABRIC_LOG_INFO("InputRouter set to GameOnly");
            } else if (!modeFlags.routeToGame && modeFlags.routeToUI) {
                inputRouter->setMode(fabric::InputMode::UIOnly);
                FABRIC_LOG_INFO("InputRouter set to UIOnly");
            } else {
                inputRouter->setMode(fabric::InputMode::GameAndUI);
                FABRIC_LOG_INFO("InputRouter set to GameAndUI");
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
        // Start in Menu mode for main menu display
        ctx.appModeManager->transition(fabric::AppMode::Menu);
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
