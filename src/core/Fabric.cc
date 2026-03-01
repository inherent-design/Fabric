#include "fabric/core/AnimationEvents.hh"
#include "fabric/core/AppContext.hh"
#include "fabric/core/Async.hh"
#include "fabric/core/AudioSystem.hh"
#include "fabric/core/BehaviorAI.hh"
#include "fabric/core/BTDebugPanel.hh"
#include "fabric/core/Camera.hh"
#include "fabric/core/CameraController.hh"
#include "fabric/core/CaveCarver.hh"
#include "fabric/core/CharacterController.hh"
#include "fabric/core/CharacterTypes.hh"
#include "fabric/core/ChunkMeshManager.hh"
#include "fabric/core/ChunkStreaming.hh"
#include "fabric/core/Constants.g.hh"
#include "fabric/core/DebrisPool.hh"
#include "fabric/core/DebugDraw.hh"
#include "fabric/core/DevConsole.hh"
#include "fabric/core/ECS.hh"
#include "fabric/core/Event.hh"
#include "fabric/core/FieldLayer.hh"
#include "fabric/core/FlightController.hh"
#include "fabric/core/InputManager.hh"
#include "fabric/core/InputRouter.hh"
#include "fabric/core/Log.hh"
#include "fabric/core/MovementFSM.hh"
#include "fabric/core/ParticleSystem.hh"
#include "fabric/core/Pathfinding.hh"
#include "fabric/core/PhysicsWorld.hh"
#include "fabric/core/Ragdoll.hh"
#include "fabric/core/Rendering.hh"
#include "fabric/core/ResourceHub.hh"
#include "fabric/core/SaveManager.hh"
#include "fabric/core/SceneView.hh"
#include "fabric/core/ShadowSystem.hh"
#include "fabric/core/Spatial.hh"
#include "fabric/core/Temporal.hh"
#include "fabric/core/TerrainGenerator.hh"
#include "fabric/core/VoxelInteraction.hh"
#include "fabric/core/VoxelMesher.hh"
#include "fabric/core/VoxelRenderer.hh"
#include "fabric/parser/ArgumentParser.hh"
#include "fabric/ui/BgfxRenderInterface.hh"
#include "fabric/ui/BgfxSystemInterface.hh"
#include "fabric/ui/DebugHUD.hh"
#include "fabric/ui/ToastManager.hh"
#include "fabric/utils/BVH.hh"
#include "fabric/utils/Profiler.hh"

#include <RmlUi/Core.h>

#include <bgfx/bgfx.h>
#include <bgfx/platform.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_properties.h>

#include <chrono>
#include <cmath>
#include <iostream>
#include <limits>
#include <unordered_map>
#include <unordered_set>

namespace {

bgfx::PlatformData getPlatformData(SDL_Window* window) {
    bgfx::PlatformData pd{};
    SDL_PropertiesID props = SDL_GetWindowProperties(window);

#if defined(SDL_PLATFORM_WIN32)
    pd.nwh = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);
#elif defined(SDL_PLATFORM_MACOS)
    pd.nwh = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_COCOA_WINDOW_POINTER, nullptr);
#elif defined(SDL_PLATFORM_LINUX)
    void* wl = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER, nullptr);
    if (wl) {
        pd.ndt = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WAYLAND_DISPLAY_POINTER, nullptr);
        pd.nwh = wl;
        pd.type = bgfx::NativeWindowHandleType::Wayland;
    } else {
        pd.ndt = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_X11_DISPLAY_POINTER, nullptr);
        pd.nwh = reinterpret_cast<void*>(
            static_cast<uintptr_t>(SDL_GetNumberProperty(props, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0)));
    }
#endif

    return pd;
}

// Upload CPU mesh data to GPU via bgfx handles
fabric::ChunkMesh uploadChunkMesh(const fabric::ChunkMeshData& data) {
    fabric::ChunkMesh mesh;
    if (data.vertices.empty())
        return mesh;

    auto layout = fabric::VoxelMesher::getVertexLayout();
    mesh.vbh = bgfx::createVertexBuffer(
        bgfx::copy(data.vertices.data(), static_cast<uint32_t>(data.vertices.size() * sizeof(fabric::VoxelVertex))),
        layout);
    mesh.ibh = bgfx::createIndexBuffer(
        bgfx::copy(data.indices.data(), static_cast<uint32_t>(data.indices.size() * sizeof(uint32_t))),
        BGFX_BUFFER_INDEX32);
    mesh.indexCount = static_cast<uint32_t>(data.indices.size());
    mesh.palette = data.palette;
    mesh.valid = true;
    return mesh;
}

// Generate terrain for a single chunk region
void generateChunkTerrain(int cx, int cy, int cz, fabric::TerrainGenerator& gen, fabric::CaveCarver& carver,
                          fabric::DensityField& density, fabric::EssenceField& essence) {
    float x0 = static_cast<float>(cx * fabric::kChunkSize);
    float y0 = static_cast<float>(cy * fabric::kChunkSize);
    float z0 = static_cast<float>(cz * fabric::kChunkSize);
    float x1 = x0 + static_cast<float>(fabric::kChunkSize);
    float y1 = y0 + static_cast<float>(fabric::kChunkSize);
    float z1 = z0 + static_cast<float>(fabric::kChunkSize);
    fabric::AABB region(fabric::Vec3f(x0, y0, z0), fabric::Vec3f(x1, y1, z1));
    gen.generate(density, essence, region);
    carver.carve(density, region);
}

} // namespace

int main(int argc, char* argv[]) {
    fabric::log::init();
    FABRIC_LOG_INFO("Starting {} {}", fabric::APP_NAME, fabric::APP_VERSION);

    fabric::ArgumentParser argParser;
    argParser.addArgument("--version", "Display version information");
    argParser.addArgument("--help", "Display help information");
    argParser.parse(argc, argv);

    if (argParser.hasArgument("--version")) {
        std::cout << fabric::APP_NAME << " version " << fabric::APP_VERSION << std::endl;
        fabric::log::shutdown();
        return 0;
    }

    if (argParser.hasArgument("--help")) {
        std::cout << "Usage: " << fabric::APP_EXECUTABLE_NAME << " [options]" << std::endl;
        std::cout << "Options:" << std::endl;
        std::cout << "  --version    Display version information" << std::endl;
        std::cout << "  --help       Display this help message" << std::endl;
        fabric::log::shutdown();
        return 0;
    }

    try {
        if (!SDL_Init(SDL_INIT_VIDEO)) {
            FABRIC_LOG_CRITICAL("SDL init failed: {}", SDL_GetError());
            fabric::log::shutdown();
            return 1;
        }

        constexpr int kWindowWidth = 1280;
        constexpr int kWindowHeight = 720;

        SDL_Window* window = SDL_CreateWindow(fabric::APP_NAME, kWindowWidth, kWindowHeight,
                                              SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_RESIZABLE);

        if (!window) {
            FABRIC_LOG_CRITICAL("Window creation failed: {}", SDL_GetError());
            SDL_Quit();
            fabric::log::shutdown();
            return 1;
        }

        // Signal single-threaded rendering before bgfx::init.
        // On macOS Metal must stay on the main thread.
        bgfx::renderFrame();

        int pw, ph;
        SDL_GetWindowSizeInPixels(window, &pw, &ph);

        bgfx::Init bgfxInit;
        bgfxInit.type = bgfx::RendererType::Count;
        bgfxInit.platformData = getPlatformData(window);
        bgfxInit.resolution.width = static_cast<uint32_t>(pw);
        bgfxInit.resolution.height = static_cast<uint32_t>(ph);
        bgfxInit.resolution.reset = BGFX_RESET_VSYNC;

        if (!bgfx::init(bgfxInit)) {
            FABRIC_LOG_CRITICAL("bgfx init failed");
            SDL_DestroyWindow(window);
            SDL_Quit();
            fabric::log::shutdown();
            return 1;
        }

        bgfx::setViewClear(0, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x303030ff, 1.0f, 0);
        bgfx::setViewRect(0, 0, 0, static_cast<uint16_t>(pw), static_cast<uint16_t>(ph));

        FABRIC_LOG_INFO("bgfx renderer: {}", bgfx::getRendererName(bgfx::getRendererType()));

        // Debug draw overlay (F4 wireframe toggle)
        fabric::DebugDraw debugDraw;
        debugDraw.init();

        // RmlUi backend interfaces
        fabric::BgfxSystemInterface rmlSystem;
        fabric::BgfxRenderInterface rmlRenderer;
        rmlRenderer.init();

        Rml::SetSystemInterface(&rmlSystem);
        Rml::SetRenderInterface(&rmlRenderer);
        Rml::Initialise();

        Rml::Context* rmlContext = Rml::CreateContext("main", Rml::Vector2i(pw, ph));
        FABRIC_LOG_INFO("RmlUi context created ({}x{})", pw, ph);

        fabric::async::init();

        //----------------------------------------------------------------------
        // Event + Input systems
        //----------------------------------------------------------------------
        fabric::EventDispatcher dispatcher;
        fabric::InputManager inputManager(dispatcher);
        fabric::InputRouter inputRouter(inputManager);
        inputRouter.setMode(fabric::InputMode::GameOnly);

        // Movement
        inputManager.bindKey("move_forward", SDLK_W);
        inputManager.bindKey("move_backward", SDLK_S);
        inputManager.bindKey("move_left", SDLK_A);
        inputManager.bindKey("move_right", SDLK_D);
        inputManager.bindKey("move_up", SDLK_SPACE);
        inputManager.bindKey("move_down", SDLK_LSHIFT);

        // Time controls
        inputManager.bindKey("time_pause", SDLK_P);
        inputManager.bindKey("time_faster", SDLK_EQUALS);
        inputManager.bindKey("time_slower", SDLK_MINUS);

        // Mode toggles
        inputManager.bindKey("toggle_fly", SDLK_F);
        inputManager.bindKey("toggle_debug", SDLK_F3);
        inputManager.bindKey("toggle_wireframe", SDLK_F4);
        inputManager.bindKey("toggle_camera", SDLK_V);
        inputManager.bindKey("toggle_collision_debug", SDLK_F10);
        inputManager.bindKey("toggle_bvh_debug", SDLK_F6);
        inputManager.bindKey("toggle_bt_debug", SDLK_F7);
        inputManager.bindKey("cycle_bt_npc", SDLK_F8);

        //----------------------------------------------------------------------
        // Timeline
        //----------------------------------------------------------------------
        fabric::Timeline timeline;

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

        //----------------------------------------------------------------------
        // Camera + Controller
        //----------------------------------------------------------------------
        fabric::Camera camera;
        bool homogeneousNdc = bgfx::getCaps()->homogeneousDepth;
        float aspect = static_cast<float>(pw) / static_cast<float>(ph);
        camera.setPerspective(60.0f, aspect, 0.1f, 1000.0f, homogeneousNdc);

        fabric::CameraController cameraCtrl(camera);

        //----------------------------------------------------------------------
        // ECS + SceneView + ResourceHub
        //----------------------------------------------------------------------
        fabric::World ecsWorld;
        ecsWorld.registerCoreComponents();
#ifdef FABRIC_ECS_INSPECTOR
        ecsWorld.enableInspector();
#endif
        fabric::SceneView sceneView(0, camera, ecsWorld.get());

        fabric::ResourceHub resourceHub;
        resourceHub.disableWorkerThreadsForTesting();

        fabric::AppContext appContext{ecsWorld, timeline, dispatcher, resourceHub};
        (void)appContext; // threaded through systems in future passes

        //----------------------------------------------------------------------
        // Terrain: density + essence fields, generator, cave carver
        //----------------------------------------------------------------------
        fabric::DensityField density;
        fabric::EssenceField essence;

        fabric::TerrainConfig terrainConfig;
        terrainConfig.seed = 42;
        terrainConfig.frequency = 0.02f;
        terrainConfig.octaves = 4;
        fabric::TerrainGenerator terrainGen(terrainConfig);

        fabric::CaveConfig caveConfig;
        caveConfig.seed = 42;
        fabric::CaveCarver caveCarver(caveConfig);

        //----------------------------------------------------------------------
        // Chunk mesh management (CPU side, budgeted re-meshing)
        //----------------------------------------------------------------------
        fabric::ChunkMeshManager meshManager(dispatcher, density.grid(), essence.grid());

        //----------------------------------------------------------------------
        // Chunk streaming
        //----------------------------------------------------------------------
        fabric::StreamingConfig streamConfig;
        streamConfig.baseRadius = 3;
        streamConfig.maxRadius = 5;
        streamConfig.maxLoadsPerTick = 8;
        streamConfig.maxUnloadsPerTick = 4;
        fabric::ChunkStreamingManager streaming(streamConfig);

        //----------------------------------------------------------------------
        // Voxel renderer + GPU mesh cache
        //----------------------------------------------------------------------
        fabric::VoxelRenderer voxelRenderer;

        std::unordered_map<fabric::ChunkCoord, fabric::ChunkMesh, fabric::ChunkCoordHash> gpuMeshes;
        std::unordered_set<fabric::ChunkCoord, fabric::ChunkCoordHash> gpuUploadQueue;

        // Flecs entities per chunk (BoundingBox + SceneEntity tag for frustum culling)
        std::unordered_map<fabric::ChunkCoord, flecs::entity, fabric::ChunkCoordHash> chunkEntities;

        //----------------------------------------------------------------------
        // Physics (must precede VoxelChanged handler)
        //----------------------------------------------------------------------
        fabric::PhysicsWorld physicsWorld;
        physicsWorld.init(4096, 0);

        // Invalidate GPU mesh and physics collision when voxel data changes
        dispatcher.addEventListener(fabric::kVoxelChangedEvent,
                                    [&gpuUploadQueue, &physicsWorld, &density](fabric::Event& e) {
                                        int cx = e.getData<int>("cx");
                                        int cy = e.getData<int>("cy");
                                        int cz = e.getData<int>("cz");
                                        gpuUploadQueue.insert({cx, cy, cz});
                                        physicsWorld.rebuildChunkCollision(density.grid(), cx, cy, cz);
                                    });

        //----------------------------------------------------------------------
        // Initial terrain generation + meshing
        //----------------------------------------------------------------------
        constexpr float kSpawnX = 16.0f;
        constexpr float kSpawnY = 48.0f;
        constexpr float kSpawnZ = 16.0f;

        {
            FABRIC_ZONE_SCOPED_N("initial_terrain");
            auto initLoad = streaming.update(kSpawnX, kSpawnY, kSpawnZ, 0.0f);

            for (const auto& coord : initLoad.toLoad) {
                generateChunkTerrain(coord.cx, coord.cy, coord.cz, terrainGen, caveCarver, density, essence);
                meshManager.markDirty(coord.cx, coord.cy, coord.cz);

                // Flecs entity with world-space AABB for frustum culling
                auto ent = ecsWorld.get().entity().add<fabric::SceneEntity>().set<fabric::BoundingBox>(
                    {static_cast<float>(coord.cx * fabric::kChunkSize),
                     static_cast<float>(coord.cy * fabric::kChunkSize),
                     static_cast<float>(coord.cz * fabric::kChunkSize),
                     static_cast<float>((coord.cx + 1) * fabric::kChunkSize),
                     static_cast<float>((coord.cy + 1) * fabric::kChunkSize),
                     static_cast<float>((coord.cz + 1) * fabric::kChunkSize)});
                chunkEntities[coord] = ent;
            }

            // Flush dirty chunks for initial load with bounded passes.
            constexpr int kMaxInitialRemeshPasses = 512;
            constexpr int kMaxInitialNoProgressPasses = 8;

            size_t previousDirty = std::numeric_limits<size_t>::max();
            int noProgressPasses = 0;
            int totalRemeshed = 0;

            for (int pass = 0; pass < kMaxInitialRemeshPasses; ++pass) {
                size_t dirtyBefore = meshManager.dirtyCount();
                if (dirtyBefore == 0) {
                    break;
                }

                int remeshed = meshManager.update();
                totalRemeshed += remeshed;

                size_t dirtyAfter = meshManager.dirtyCount();
                if (dirtyAfter >= dirtyBefore || dirtyAfter >= previousDirty) {
                    ++noProgressPasses;
                } else {
                    noProgressPasses = 0;
                }
                previousDirty = dirtyAfter;

                if (noProgressPasses >= kMaxInitialNoProgressPasses) {
                    FABRIC_LOG_WARN(
                        "Initial terrain remesh made no progress for {} passes; deferring {} chunks to runtime",
                        noProgressPasses, dirtyAfter);
                    break;
                }
            }

            // Upload all ready initial meshes to GPU. Any deferred dirty chunks stay queued for runtime sync.
            for (const auto& coord : initLoad.toLoad) {
                if (meshManager.isDirty(coord)) {
                    continue;
                }

                const auto* data = meshManager.meshFor(coord);
                if (data && !data->vertices.empty()) {
                    gpuMeshes[coord] = uploadChunkMesh(*data);
                }
            }

            FABRIC_LOG_INFO(
                "Initial terrain: {} chunks loaded, {} remeshed, {} GPU meshes, {} chunks pending runtime remesh",
                initLoad.toLoad.size(), totalRemeshed, gpuMeshes.size(), meshManager.dirtyCount());
        }

        //----------------------------------------------------------------------
        // Character systems
        //----------------------------------------------------------------------
        constexpr float kCharWidth = 0.6f;
        constexpr float kCharHeight = 1.8f;
        constexpr float kCharDepth = 0.6f;

        fabric::CharacterController charCtrl(kCharWidth, kCharHeight, kCharDepth);
        fabric::FlightController flightCtrl(kCharWidth, kCharHeight, kCharDepth);
        fabric::MovementFSM movementFSM;
        fabric::CharacterConfig charConfig;

        fabric::Vec3f playerPos(kSpawnX, kSpawnY, kSpawnZ);
        fabric::Velocity playerVel;

        //----------------------------------------------------------------------
        // Voxel interaction
        //----------------------------------------------------------------------
        fabric::VoxelInteraction voxelInteraction(density, essence, dispatcher);

        //----------------------------------------------------------------------
        // Shadow system
        //----------------------------------------------------------------------
        fabric::ShadowSystem shadowSystem(fabric::presetConfig(fabric::ShadowQualityPreset::Medium));

        fabric::Vec3f lightDir(0.5f, -0.8f, 0.3f);
        {
            float len = std::sqrt(lightDir.x * lightDir.x + lightDir.y * lightDir.y + lightDir.z * lightDir.z);
            lightDir = fabric::Vec3f(lightDir.x / len, lightDir.y / len, lightDir.z / len);
        }

        //----------------------------------------------------------------------
        // Sprint 7 systems: ragdoll, AI, audio
        //----------------------------------------------------------------------
        fabric::Ragdoll ragdoll;
        ragdoll.init(&physicsWorld);

        fabric::AudioSystem audioSystem;
        audioSystem.setThreadedMode(true);
        audioSystem.init();
        audioSystem.setDensityGrid(&density.grid());

        fabric::BehaviorAI behaviorAI;
        behaviorAI.init(ecsWorld.get());

        fabric::Pathfinding pathfinding;
        pathfinding.init();

        fabric::AnimationEvents animEvents;
        animEvents.init();

        //----------------------------------------------------------------------
        // Debug HUD
        //----------------------------------------------------------------------
        fabric::DebugHUD debugHUD;
        debugHUD.init(rmlContext);

        //----------------------------------------------------------------------
        // BT Debug Panel
        //----------------------------------------------------------------------
        fabric::BTDebugPanel btDebugPanel;
        btDebugPanel.init(rmlContext);
        flecs::entity btDebugSelectedNpc;

        //----------------------------------------------------------------------
        // Developer Console
        //----------------------------------------------------------------------
        fabric::DevConsole devConsole;
        devConsole.init(rmlContext);

        // Backtick toggles console and switches input mode
        inputRouter.setConsoleToggleCallback([&devConsole, &inputRouter]() {
            devConsole.toggle();
            if (devConsole.isVisible()) {
                inputRouter.setMode(fabric::InputMode::UIOnly);
            } else {
                inputRouter.setMode(fabric::InputMode::GameOnly);
            }
        });

        //----------------------------------------------------------------------
        // Save system + toast notifications
        //----------------------------------------------------------------------
        fabric::SaveManager saveManager("saves");
        fabric::SceneSerializer saveSerializer;
        fabric::ToastManager toastManager;

        // F5 = quicksave, F9 = quickload (via InputRouter key callbacks)
        inputRouter.registerKeyCallback(SDLK_F5, [&]() {
            fabric::SceneSerializer qsSerializer;
            if (saveManager.save(
                    "quicksave", qsSerializer, ecsWorld, density, essence, timeline,
                    std::optional<fabric::Position>(fabric::Position{playerPos.x, playerPos.y, playerPos.z}),
                    std::optional<fabric::Position>(fabric::Position{playerVel.x, playerVel.y, playerVel.z}))) {
                toastManager.show("Quick save complete", 2.0f);
                FABRIC_LOG_INFO("Quick save complete");
            } else {
                toastManager.show("Quick save failed", 3.0f);
                FABRIC_LOG_ERROR("Quick save failed");
            }
        });

        inputRouter.registerKeyCallback(SDLK_F9, [&]() {
            fabric::SceneSerializer qlSerializer;
            std::optional<fabric::Position> loadedPos;
            std::optional<fabric::Position> loadedVel;
            if (saveManager.load("quicksave", qlSerializer, ecsWorld, density, essence, timeline, loadedPos,
                                 loadedVel)) {
                if (loadedPos) {
                    playerPos = fabric::Vec3f(loadedPos->x, loadedPos->y, loadedPos->z);
                }
                if (loadedVel) {
                    playerVel = fabric::Velocity{loadedVel->x, loadedVel->y, loadedVel->z};
                }
                toastManager.show("Quick load complete", 2.0f);
                FABRIC_LOG_INFO("Quick load complete");
            } else {
                toastManager.show("Quick load failed", 3.0f);
                FABRIC_LOG_ERROR("Quick load failed");
            }
        });

        //----------------------------------------------------------------------
        // Particle system + DebrisPool emitter wiring
        //----------------------------------------------------------------------
        fabric::ParticleSystem particleSystem;
        particleSystem.init();

        fabric::DebrisPool debrisPool;
        debrisPool.enableParticleConversion(true);
        debrisPool.setParticleEmitter(
            [&particleSystem](const fabric::Vector3<float, fabric::Space::World>& pos, float radius, int count) {
                particleSystem.emit(pos, radius, count, fabric::ParticleType::DebrisPuff);
            });

        //----------------------------------------------------------------------
        // Toggle event handlers
        //----------------------------------------------------------------------
        dispatcher.addEventListener("toggle_fly", [&](fabric::Event&) {
            if (movementFSM.isFlying()) {
                movementFSM.tryTransition(fabric::CharacterState::Falling);
                FABRIC_LOG_INFO("Flight mode: off");
            } else {
                movementFSM.tryTransition(fabric::CharacterState::Flying);
                playerVel = {};
                FABRIC_LOG_INFO("Flight mode: on");
            }
        });

        dispatcher.addEventListener("toggle_debug", [&](fabric::Event&) { debugHUD.toggle(); });

        dispatcher.addEventListener("toggle_wireframe", [&](fabric::Event&) {
            debugDraw.toggleWireframe();
            FABRIC_LOG_INFO("Wireframe: {}", debugDraw.isWireframeEnabled() ? "on" : "off");
        });

        dispatcher.addEventListener("toggle_camera", [&](fabric::Event&) {
            if (cameraCtrl.mode() == fabric::CameraMode::FirstPerson) {
                cameraCtrl.setMode(fabric::CameraMode::ThirdPerson);
            } else {
                cameraCtrl.setMode(fabric::CameraMode::FirstPerson);
            }
        });

        dispatcher.addEventListener("toggle_collision_debug", [&](fabric::Event&) {
            debugDraw.toggleFlag(fabric::DebugDrawFlags::CollisionShapes);
            FABRIC_LOG_INFO("Collision shapes: {}",
                            debugDraw.hasFlag(fabric::DebugDrawFlags::CollisionShapes) ? "on" : "off");
        });

        dispatcher.addEventListener("toggle_bvh_debug", [&](fabric::Event&) {
            debugDraw.toggleFlag(fabric::DebugDrawFlags::BVHOverlay);
            FABRIC_LOG_INFO("BVH overlay: {}", debugDraw.hasFlag(fabric::DebugDrawFlags::BVHOverlay) ? "on" : "off");
        });

        dispatcher.addEventListener("toggle_bt_debug", [&](fabric::Event&) {
            btDebugPanel.toggle();
            FABRIC_LOG_INFO("BT Debug: {}", btDebugPanel.isVisible() ? "on" : "off");
        });

        dispatcher.addEventListener("cycle_bt_npc", [&](fabric::Event&) {
            btDebugPanel.selectNextNPC(behaviorAI, ecsWorld.get());
            btDebugSelectedNpc = btDebugPanel.selectedNpc();
        });

        // Jump on space press (grounded only; in flight, move_up is continuous)
        dispatcher.addEventListener("move_up", [&](fabric::Event&) {
            if (movementFSM.isGrounded()) {
                movementFSM.tryTransition(fabric::CharacterState::Jumping);
                playerVel.y = charConfig.jumpForce;
            }
        });

        float interactionCooldown = 0.0f;
        constexpr float kInteractionRate = 0.15f;

        FABRIC_LOG_INFO("All systems initialized");

        //----------------------------------------------------------------------
        // Main loop
        //----------------------------------------------------------------------
        constexpr double kFixedDt = 1.0 / 60.0;
        double accumulator = 0.0;
        auto lastTime = std::chrono::high_resolution_clock::now();
        bool running = true;
        devConsole.setQuitCallback([&running]() { running = false; });

        FABRIC_LOG_INFO("Entering main loop");

        while (running) {
            FABRIC_ZONE_SCOPED_N("main_loop");

            auto now = std::chrono::high_resolution_clock::now();
            double frameTime = std::chrono::duration<double>(now - lastTime).count();
            lastTime = now;

            if (frameTime > 0.25)
                frameTime = 0.25;
            accumulator += frameTime;

            // Route SDL events through InputRouter (Escape toggles UI mode)
            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                inputRouter.routeEvent(event, rmlContext);

                if (event.type == SDL_EVENT_QUIT)
                    running = false;

                if (event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
                    auto w = static_cast<uint32_t>(event.window.data1);
                    auto h = static_cast<uint32_t>(event.window.data2);
                    if (w == 0 || h == 0)
                        continue;
                    bgfx::reset(w, h, BGFX_RESET_VSYNC);
                    bgfx::setViewRect(0, 0, 0, static_cast<uint16_t>(w), static_cast<uint16_t>(h));
                    float newAspect = static_cast<float>(w) / static_cast<float>(h);
                    camera.setPerspective(60.0f, newAspect, 0.1f, 1000.0f, homogeneousNdc);
                    rmlContext->SetDimensions(Rml::Vector2i(static_cast<int>(w), static_cast<int>(h)));
                }
            }

            // Mouse look (once per frame, not per fixed step)
            cameraCtrl.processMouseInput(inputManager.mouseDeltaX(), inputManager.mouseDeltaY());

            //------------------------------------------------------------------
            // Fixed timestep
            //------------------------------------------------------------------
            while (accumulator >= kFixedDt) {
                float dt = static_cast<float>(kFixedDt);

                fabric::async::poll();
                timeline.update(kFixedDt);

                saveManager.tickAutosave(
                    dt, saveSerializer, ecsWorld, density, essence, timeline,
                    std::optional<fabric::Position>(fabric::Position{playerPos.x, playerPos.y, playerPos.z}),
                    std::optional<fabric::Position>(fabric::Position{playerVel.x, playerVel.y, playerVel.z}));
                toastManager.update(dt);

                // Streaming: load/unload chunks around player
                float speed =
                    std::sqrt(playerVel.x * playerVel.x + playerVel.y * playerVel.y + playerVel.z * playerVel.z);
                auto streamUpdate = streaming.update(playerPos.x, playerPos.y, playerPos.z, speed);

                for (const auto& coord : streamUpdate.toLoad) {
                    generateChunkTerrain(coord.cx, coord.cy, coord.cz, terrainGen, caveCarver, density, essence);
                    meshManager.markDirty(coord.cx, coord.cy, coord.cz);
                    gpuUploadQueue.insert(coord);

                    if (chunkEntities.find(coord) == chunkEntities.end()) {
                        auto ent = ecsWorld.get().entity().add<fabric::SceneEntity>().set<fabric::BoundingBox>(
                            {static_cast<float>(coord.cx * fabric::kChunkSize),
                             static_cast<float>(coord.cy * fabric::kChunkSize),
                             static_cast<float>(coord.cz * fabric::kChunkSize),
                             static_cast<float>((coord.cx + 1) * fabric::kChunkSize),
                             static_cast<float>((coord.cy + 1) * fabric::kChunkSize),
                             static_cast<float>((coord.cz + 1) * fabric::kChunkSize)});
                        chunkEntities[coord] = ent;
                    }
                }

                for (const auto& coord : streamUpdate.toUnload) {
                    gpuUploadQueue.erase(coord);
                    meshManager.removeChunk(coord);
                    physicsWorld.removeChunkCollision(coord.cx, coord.cy, coord.cz);

                    if (auto it = chunkEntities.find(coord); it != chunkEntities.end()) {
                        it->second.destruct();
                        chunkEntities.erase(it);
                    }
                    if (auto it = gpuMeshes.find(coord); it != gpuMeshes.end()) {
                        fabric::VoxelMesher::destroyMesh(it->second);
                        gpuMeshes.erase(it);
                    }
                    density.grid().removeChunk(coord.cx, coord.cy, coord.cz);
                    essence.grid().removeChunk(coord.cx, coord.cy, coord.cz);
                }

                // Character movement
                if (movementFSM.isFlying()) {
                    auto fwd = cameraCtrl.forward();
                    auto right = cameraCtrl.right();

                    fabric::Vec3f moveDir(0.0f, 0.0f, 0.0f);
                    if (inputManager.isActionActive("move_forward"))
                        moveDir = moveDir + fwd;
                    if (inputManager.isActionActive("move_backward"))
                        moveDir = moveDir - fwd;
                    if (inputManager.isActionActive("move_right"))
                        moveDir = moveDir + right;
                    if (inputManager.isActionActive("move_left"))
                        moveDir = moveDir - right;
                    if (inputManager.isActionActive("move_up"))
                        moveDir = moveDir + fabric::Vec3f(0.0f, 1.0f, 0.0f);
                    if (inputManager.isActionActive("move_down"))
                        moveDir = moveDir - fabric::Vec3f(0.0f, 1.0f, 0.0f);

                    float len = std::sqrt(moveDir.x * moveDir.x + moveDir.y * moveDir.y + moveDir.z * moveDir.z);
                    if (len > 0.001f)
                        moveDir = fabric::Vec3f(moveDir.x / len, moveDir.y / len, moveDir.z / len);

                    fabric::Vec3f displacement(moveDir.x * charConfig.flightSpeed * dt,
                                               moveDir.y * charConfig.flightSpeed * dt,
                                               moveDir.z * charConfig.flightSpeed * dt);

                    auto result = flightCtrl.move(playerPos, displacement, density.grid());
                    playerPos = result.resolvedPosition;

                } else {
                    // Ground mode: flatten forward/right to XZ plane
                    auto fwd = cameraCtrl.forward();
                    auto right = cameraCtrl.right();

                    fabric::Vec3f flatFwd(fwd.x, 0.0f, fwd.z);
                    float fwdLen = std::sqrt(flatFwd.x * flatFwd.x + flatFwd.z * flatFwd.z);
                    if (fwdLen > 0.001f)
                        flatFwd = fabric::Vec3f(flatFwd.x / fwdLen, 0.0f, flatFwd.z / fwdLen);

                    fabric::Vec3f flatRight(right.x, 0.0f, right.z);
                    float rightLen = std::sqrt(flatRight.x * flatRight.x + flatRight.z * flatRight.z);
                    if (rightLen > 0.001f)
                        flatRight = fabric::Vec3f(flatRight.x / rightLen, 0.0f, flatRight.z / rightLen);

                    fabric::Vec3f horizMove(0.0f, 0.0f, 0.0f);
                    if (inputManager.isActionActive("move_forward"))
                        horizMove = horizMove + flatFwd;
                    if (inputManager.isActionActive("move_backward"))
                        horizMove = horizMove - flatFwd;
                    if (inputManager.isActionActive("move_right"))
                        horizMove = horizMove + flatRight;
                    if (inputManager.isActionActive("move_left"))
                        horizMove = horizMove - flatRight;

                    float horizLen = std::sqrt(horizMove.x * horizMove.x + horizMove.z * horizMove.z);
                    if (horizLen > 0.001f)
                        horizMove = fabric::Vec3f(horizMove.x / horizLen, 0.0f, horizMove.z / horizLen);

                    // Gravity
                    playerVel.y -= charConfig.gravity * dt;

                    fabric::Vec3f displacement(horizMove.x * charConfig.walkSpeed * dt, playerVel.y * dt,
                                               horizMove.z * charConfig.walkSpeed * dt);

                    auto result = charCtrl.move(playerPos, displacement, density.grid());
                    playerPos = result.resolvedPosition;

                    if (result.onGround) {
                        playerVel.y = 0.0f;
                        if (movementFSM.currentState() == fabric::CharacterState::Falling ||
                            movementFSM.currentState() == fabric::CharacterState::Jumping) {
                            movementFSM.tryTransition(fabric::CharacterState::Grounded);
                        }
                    } else if (movementFSM.isGrounded()) {
                        movementFSM.tryTransition(fabric::CharacterState::Falling);
                    }

                    // Ceiling collision: kill upward velocity
                    if (result.hitY && playerVel.y > 0.0f)
                        playerVel.y = 0.0f;
                }

                // Voxel interaction (mouse buttons)
                interactionCooldown -= dt;
                if (interactionCooldown <= 0.0f) {
                    auto camPos = cameraCtrl.position();
                    auto camFwd = cameraCtrl.forward();

                    if (inputManager.mouseButton(1)) {
                        auto r = voxelInteraction.destroyMatterAt(density.grid(), camPos.x, camPos.y, camPos.z,
                                                                  camFwd.x, camFwd.y, camFwd.z, 10.0f);
                        if (r.success)
                            interactionCooldown = kInteractionRate;
                    }
                    if (inputManager.mouseButton(3)) {
                        auto r = voxelInteraction.createMatterAt(
                            density.grid(), camPos.x, camPos.y, camPos.z, camFwd.x, camFwd.y, camFwd.z, 1.0f,
                            fabric::Vector4<float, fabric::Space::World>(0.4f, 0.7f, 0.3f, 1.0f), 10.0f);
                        if (r.success)
                            interactionCooldown = kInteractionRate;
                    }
                }

                // Physics and AI step at fixed rate
                physicsWorld.step(dt);
                behaviorAI.update(dt);

                // LOD: compute per-chunk LOD from camera distance (marks dirty on change)
                {
                    auto camPos = cameraCtrl.position();
                    meshManager.updateLOD(camPos.x, camPos.y, camPos.z);
                }

                // Mesh manager: budgeted CPU re-meshing of dirty chunks
                meshManager.update();

                // Particle simulation + debris-to-particle conversion
                debrisPool.update(dt);
                particleSystem.update(dt);

                // GPU mesh sync: upload re-meshed chunks
                {
                    auto it = gpuUploadQueue.begin();
                    while (it != gpuUploadQueue.end()) {
                        if (chunkEntities.find(*it) == chunkEntities.end()) {
                            it = gpuUploadQueue.erase(it);
                            continue;
                        }

                        if (!meshManager.isDirty(*it)) {
                            const auto* data = meshManager.meshFor(*it);
                            if (auto git = gpuMeshes.find(*it); git != gpuMeshes.end()) {
                                fabric::VoxelMesher::destroyMesh(git->second);
                                gpuMeshes.erase(git);
                            }
                            if (data && !data->vertices.empty()) {
                                gpuMeshes[*it] = uploadChunkMesh(*data);
                            }
                            it = gpuUploadQueue.erase(it);
                        } else {
                            ++it;
                        }
                    }
                }

                accumulator -= kFixedDt;
            }

            // Camera tracks player position (spring arm collision for 3P mode)
            cameraCtrl.update(playerPos, static_cast<float>(frameTime), &density.grid());

            // Audio listener follows camera, update per frame for smooth spatial audio
            audioSystem.setListenerPosition(cameraCtrl.position());
            audioSystem.setListenerDirection(cameraCtrl.forward(), cameraCtrl.up());
            audioSystem.update(static_cast<float>(frameTime));

            // Per-frame resets (InputRouter delegates to InputManager)
            inputRouter.beginFrame();

            // Shadow cascade computation
            shadowSystem.update(camera,
                                fabric::Vector3<float, fabric::Space::World>(lightDir.x, lightDir.y, lightDir.z));
            voxelRenderer.setLightDirection(
                fabric::Vector3<float, fabric::Space::World>(lightDir.x, lightDir.y, lightDir.z));

            //------------------------------------------------------------------
            // Render
            //------------------------------------------------------------------
            {
                FABRIC_ZONE_SCOPED_N("render_submit");

                // Apply debug overlay state (wireframe toggle)
                debugDraw.applyDebugFlags();

                // ECS entity rendering (SceneView: cull, build render list, submit)
                sceneView.render();

                std::unordered_set<flecs::entity_t> visibleEntityIds;
                visibleEntityIds.reserve(sceneView.visibleEntities().size());
                for (const auto& entity : sceneView.visibleEntities()) {
                    visibleEntityIds.insert(entity.id());
                }

                // Voxel chunk rendering (frustum-filtered via chunk entities)
                for (const auto& [coord, mesh] : gpuMeshes) {
                    auto entIt = chunkEntities.find(coord);
                    if (entIt == chunkEntities.end())
                        continue;
                    if (visibleEntityIds.find(entIt->second.id()) == visibleEntityIds.end())
                        continue;
                    voxelRenderer.render(sceneView.geometryViewId(), mesh, coord.cx, coord.cy, coord.cz);
                }

                // Particle billboard rendering (dedicated view, alpha blended)
                {
                    int curPW, curPH;
                    SDL_GetWindowSizeInPixels(window, &curPW, &curPH);
                    particleSystem.render(camera.viewMatrix(), camera.projectionMatrix(), static_cast<uint16_t>(curPW),
                                          static_cast<uint16_t>(curPH));
                }

                // Debug draw overlay (lines, shapes) on geometry view
                debugDraw.begin(sceneView.geometryViewId());

                // Collision shape overlays (F10)
                if (debugDraw.hasFlag(fabric::DebugDrawFlags::CollisionShapes)) {
                    debugDraw.setColor(0xff00ff00); // green (ABGR)
                    for (const auto& [coord, ent] : chunkEntities) {
                        if (physicsWorld.chunkCollisionShapeCount(coord.cx, coord.cy, coord.cz) > 0) {
                            float x0 = static_cast<float>(coord.cx * fabric::kChunkSize);
                            float y0 = static_cast<float>(coord.cy * fabric::kChunkSize);
                            float z0 = static_cast<float>(coord.cz * fabric::kChunkSize);
                            float x1 = x0 + static_cast<float>(fabric::kChunkSize);
                            float y1 = y0 + static_cast<float>(fabric::kChunkSize);
                            float z1 = z0 + static_cast<float>(fabric::kChunkSize);
                            debugDraw.drawWireBox(x0, y0, z0, x1, y1, z1);
                        }
                    }
                }

                // BVH overlay with depth coloring (F6)
                if (debugDraw.hasFlag(fabric::DebugDrawFlags::BVHOverlay)) {
                    fabric::BVH<int> chunkBVH;
                    int idx = 0;
                    for (const auto& [coord, ent] : chunkEntities) {
                        float x0 = static_cast<float>(coord.cx * fabric::kChunkSize);
                        float y0 = static_cast<float>(coord.cy * fabric::kChunkSize);
                        float z0 = static_cast<float>(coord.cz * fabric::kChunkSize);
                        float x1 = x0 + static_cast<float>(fabric::kChunkSize);
                        float y1 = y0 + static_cast<float>(fabric::kChunkSize);
                        float z1 = z0 + static_cast<float>(fabric::kChunkSize);
                        chunkBVH.insert(fabric::AABB(fabric::Vec3f(x0, y0, z0), fabric::Vec3f(x1, y1, z1)), idx++);
                    }
                    chunkBVH.build();

                    constexpr float kMaxVisDepth = 8.0f;
                    chunkBVH.visitNodes([&](const fabric::AABB& bounds, int depth, bool isLeaf) {
                        uint32_t color;
                        if (isLeaf) {
                            color = 0xff00ffff; // yellow for leaves (ABGR)
                        } else {
                            float t = std::min(1.0f, static_cast<float>(depth) / kMaxVisDepth);
                            auto r = static_cast<uint8_t>((1.0f - t) * 255);
                            auto b = static_cast<uint8_t>(t * 255);
                            color = 0xff000000u | (static_cast<uint32_t>(b) << 16) | static_cast<uint32_t>(r);
                        }
                        debugDraw.setColor(color);
                        debugDraw.drawWireBox(bounds.min.x, bounds.min.y, bounds.min.z, bounds.max.x, bounds.max.y,
                                              bounds.max.z);
                    });
                }

                debugDraw.end();

                // RmlUi overlay (view 255, after 3D scene, before frame flip)
                int curW, curH;
                SDL_GetWindowSizeInPixels(window, &curW, &curH);
                rmlRenderer.beginFrame(static_cast<uint16_t>(curW), static_cast<uint16_t>(curH));
                rmlContext->Update();
                rmlContext->Render();

                bgfx::frame();
            }

            // Debug HUD data update (after render, before next frame)
            {
                fabric::DebugData debugData;
                debugData.fps = (frameTime > 0.0) ? static_cast<float>(1.0 / frameTime) : 0.0f;
                debugData.frameTimeMs = static_cast<float>(frameTime * 1000.0);
                debugData.visibleChunks = static_cast<int>(gpuMeshes.size());
                debugData.totalChunks = static_cast<int>(density.grid().chunkCount());
                debugData.cameraPosition = cameraCtrl.position();
                debugData.currentRadius = streaming.currentRadius();
                debugData.currentState = fabric::MovementFSM::stateToString(movementFSM.currentState());

                int triCount = 0;
                for (const auto& [_, m] : gpuMeshes)
                    triCount += static_cast<int>(m.indexCount / 3);
                debugData.triangleCount = triCount;

                // Perf overlay (EF-18): bgfx stats (called after bgfx::frame())
                const bgfx::Stats* stats = bgfx::getStats();
                debugData.drawCallCount = static_cast<int>(stats->numDraw);
                if (stats->gpuTimerFreq > 0) {
                    double gpuMs = 1000.0 * static_cast<double>(stats->gpuTimeEnd - stats->gpuTimeBegin) /
                                   static_cast<double>(stats->gpuTimerFreq);
                    debugData.gpuTimeMs = static_cast<float>(gpuMs);
                }
                debugData.memoryUsageMB =
                    static_cast<float>(stats->textureMemoryUsed + stats->rtMemoryUsed) / (1024.0f * 1024.0f);

                // Subsystem counts
                if (auto* jolt = physicsWorld.joltSystem()) {
                    debugData.physicsBodyCount = static_cast<int>(jolt->GetNumBodies());
                }
                debugData.audioVoiceCount = static_cast<int>(audioSystem.activeSoundCount());
                debugData.chunkMeshQueueSize = static_cast<int>(meshManager.dirtyCount());

                debugHUD.update(debugData);
            }

            if (btDebugPanel.isVisible()) {
                btDebugPanel.update(behaviorAI, btDebugSelectedNpc);
            }

            FABRIC_FRAME_MARK;
        }

        //----------------------------------------------------------------------
        // Shutdown (reverse initialization order)
        //----------------------------------------------------------------------
        FABRIC_LOG_INFO("Shutting down");

        // saveManager has no shutdown (value type, RAII)

        devConsole.shutdown();
        btDebugPanel.shutdown();
        debugHUD.shutdown();

        animEvents.shutdown();
        pathfinding.shutdown();
        behaviorAI.shutdown();
        audioSystem.shutdown();
        ragdoll.shutdown();
        physicsWorld.shutdown();

        for (auto& [_, mesh] : gpuMeshes) {
            fabric::VoxelMesher::destroyMesh(mesh);
        }
        gpuMeshes.clear();

        for (auto& [_, entity] : chunkEntities) {
            entity.destruct();
        }
        chunkEntities.clear();

        Rml::Shutdown();
        rmlRenderer.shutdown();

        particleSystem.shutdown();
        voxelRenderer.shutdown();
        sceneView.skyRenderer().shutdown();
        debugDraw.shutdown();
        bgfx::shutdown();
        SDL_DestroyWindow(window);
        SDL_Quit();
        fabric::async::shutdown();
        fabric::log::shutdown();

        return 0;

    } catch (const std::exception& e) {
        FABRIC_LOG_ERROR("Fatal: {}", e.what());
        fabric::log::shutdown();
        return 1;
    }
}
