// Recurse.cc -- Main entry point for the Recurse game.
// Constructs FabricAppDesc with full game logic and delegates to
// FabricApp::run(). All game systems live in RecurseState, shared
// across callbacks via lambda captures.

#include "fabric/core/AppContext.hh"
#include "fabric/core/AppModeManager.hh"
#include "fabric/core/Camera.hh"
#include "fabric/core/ConfigManager.hh"
#include "fabric/core/Constants.g.hh"
#include "fabric/core/ECS.hh"
#include "fabric/core/Event.hh"
#include "fabric/core/FabricApp.hh"
#include "fabric/core/FabricAppDesc.hh"
#include "fabric/core/FieldLayer.hh"
#include "fabric/core/InputManager.hh"
#include "fabric/core/InputRouter.hh"
#include "fabric/core/Log.hh"
#include "fabric/core/Rendering.hh"
#include "fabric/core/ResourceHub.hh"
#include "fabric/core/SceneView.hh"
#include "fabric/core/Spatial.hh"
#include "fabric/core/SystemRegistry.hh"
#include "fabric/core/Temporal.hh"

#include "recurse/ai/BehaviorAI.hh"
#include "recurse/ai/BTDebugPanel.hh"
#include "recurse/ai/Pathfinding.hh"
#include "recurse/animation/AnimationEvents.hh"
#include "recurse/audio/AudioSystem.hh"
#include "recurse/gameplay/CameraController.hh"
#include "recurse/gameplay/CharacterController.hh"
#include "recurse/gameplay/CharacterTypes.hh"
#include "recurse/gameplay/FlightController.hh"
#include "recurse/gameplay/MovementFSM.hh"
#include "recurse/gameplay/VoxelInteraction.hh"
#include "recurse/persistence/SaveManager.hh"
#include "recurse/physics/PhysicsWorld.hh"
#include "recurse/physics/Ragdoll.hh"
#include "recurse/render/DebugDraw.hh"
#include "recurse/render/OITCompositor.hh"
#include "recurse/render/ParticleSystem.hh"
#include "recurse/render/ShadowSystem.hh"
#include "recurse/render/VoxelRenderer.hh"
#include "recurse/ui/ContentBrowser.hh"
#include "recurse/ui/DebrisPool.hh"
#include "recurse/ui/DevConsole.hh"
#include "recurse/world/CaveCarver.hh"
#include "recurse/world/ChunkMeshManager.hh"
#include "recurse/world/ChunkStreaming.hh"
#include "recurse/world/TerrainGenerator.hh"
#include "recurse/world/VoxelMesher.hh"

#include "fabric/ui/DebugHUD.hh"
#include "fabric/ui/ToastManager.hh"
#include "fabric/utils/BVH.hh"
#include "fabric/utils/Profiler.hh"

#include <bgfx/bgfx.h>
#include <SDL3/SDL.h>

#include <cmath>
#include <limits>
#include <memory>
#include <unordered_map>
#include <unordered_set>

namespace {

constexpr float kSpawnX = 16.0f;
constexpr float kSpawnY = 48.0f;
constexpr float kSpawnZ = 16.0f;
constexpr float kInteractionRate = 0.15f;

// Upload CPU mesh data to GPU via bgfx handles.
// Application-specific helper (not engine concern).
recurse::ChunkMesh uploadChunkMesh(const recurse::ChunkMeshData& data) {
    recurse::ChunkMesh mesh;
    if (data.vertices.empty())
        return mesh;

    auto layout = recurse::VoxelMesher::getVertexLayout();
    mesh.vbh = bgfx::createVertexBuffer(
        bgfx::copy(data.vertices.data(), static_cast<uint32_t>(data.vertices.size() * sizeof(recurse::VoxelVertex))),
        layout);
    mesh.ibh = bgfx::createIndexBuffer(
        bgfx::copy(data.indices.data(), static_cast<uint32_t>(data.indices.size() * sizeof(uint32_t))),
        BGFX_BUFFER_INDEX32);
    mesh.indexCount = static_cast<uint32_t>(data.indices.size());
    mesh.palette = data.palette;
    mesh.valid = true;
    return mesh;
}

// Generate terrain for a single chunk region.
// Application-specific helper (not engine concern).
void generateChunkTerrain(int cx, int cy, int cz, recurse::TerrainGenerator& gen, recurse::CaveCarver& carver,
                          fabric::DensityField& density, fabric::EssenceField& essence) {
    float x0 = static_cast<float>(cx * recurse::kChunkSize);
    float y0 = static_cast<float>(cy * recurse::kChunkSize);
    float z0 = static_cast<float>(cz * recurse::kChunkSize);
    float x1 = x0 + static_cast<float>(recurse::kChunkSize);
    float y1 = y0 + static_cast<float>(recurse::kChunkSize);
    float z1 = z0 + static_cast<float>(recurse::kChunkSize);
    fabric::AABB region(fabric::Vec3f(x0, y0, z0), fabric::Vec3f(x1, y1, z1));
    gen.generate(density, essence, region);
    carver.carve(density, region);
}

/// Persistent game state shared across onInit/onFixedUpdate/onRender/onShutdown
/// via std::shared_ptr captured by lambda closures.
struct RecurseState {
    // Terrain
    fabric::DensityField density;
    fabric::EssenceField essence;
    std::unique_ptr<recurse::TerrainGenerator> terrainGen;
    std::unique_ptr<recurse::CaveCarver> caveCarver;

    // Chunk management
    std::unique_ptr<recurse::ChunkMeshManager> meshManager;
    std::unique_ptr<recurse::ChunkStreamingManager> streaming;

    // Rendering
    recurse::VoxelRenderer voxelRenderer;
    std::unordered_map<recurse::ChunkCoord, recurse::ChunkMesh, recurse::ChunkCoordHash> gpuMeshes;
    std::unordered_set<recurse::ChunkCoord, recurse::ChunkCoordHash> gpuUploadQueue;
    std::unordered_map<recurse::ChunkCoord, flecs::entity, recurse::ChunkCoordHash> chunkEntities;

    // Physics
    recurse::PhysicsWorld physicsWorld;

    // Camera
    std::unique_ptr<recurse::CameraController> cameraCtrl;

    // Character
    std::unique_ptr<recurse::CharacterController> charCtrl;
    std::unique_ptr<recurse::FlightController> flightCtrl;
    recurse::MovementFSM movementFSM;
    recurse::CharacterConfig charConfig;
    fabric::Vec3f playerPos{kSpawnX, kSpawnY, kSpawnZ};
    recurse::Velocity playerVel{};

    // Voxel interaction
    std::unique_ptr<recurse::VoxelInteraction> voxelInteraction;
    float interactionCooldown = 0.0f;

    // Shadow
    std::unique_ptr<recurse::ShadowSystem> shadowSystem;
    fabric::Vec3f lightDir{};

    // OIT
    recurse::OITCompositor oitCompositor;

    // AI, Audio, Animation
    recurse::Ragdoll ragdoll;
    recurse::AudioSystem audioSystem;
    recurse::BehaviorAI behaviorAI;
    recurse::Pathfinding pathfinding;
    recurse::AnimationEvents animEvents;

    // Debug / UI
    recurse::DebugDraw debugDraw;
    fabric::DebugHUD debugHUD;
    recurse::BTDebugPanel btDebugPanel;
    flecs::entity btDebugSelectedNpc;
    recurse::ContentBrowser contentBrowser;
    recurse::DevConsole devConsole;

    // Save system
    std::unique_ptr<recurse::SaveManager> saveManager;
    recurse::SceneSerializer saveSerializer;
    fabric::ToastManager toastManager;

    // Particles
    recurse::ParticleSystem particleSystem;
    recurse::DebrisPool debrisPool;

    // Per-frame tracking
    int frameCounter = 0;
    bool mouseLookProcessed = false;
};

} // namespace

/// Build the FabricAppDesc for the Recurse game.
///
/// FabricApp::run() owns: log, CLI parsing, config loading, infrastructure
/// objects (EventDispatcher, Timeline, World, ResourceHub, etc.), SDL/bgfx/RmlUi
/// init, main loop (fixed timestep + render), and engine shutdown.
///
/// Recurse provides via callbacks: all game system construction (onInit),
/// per-tick simulation (onFixedUpdate), per-frame rendering (onRender),
/// and game system teardown (onShutdown).
fabric::FabricAppDesc buildRecurseDesc() {
    fabric::FabricAppDesc desc;
    desc.name = "Recurse";
    desc.configPath = "recurse.toml";
    desc.headless = false;

    auto state = std::make_shared<RecurseState>();

    // -----------------------------------------------------------------
    // onInit: construct all game systems, bind keys, register listeners
    // -----------------------------------------------------------------
    desc.onInit = [state](fabric::AppContext& ctx) {
        FABRIC_LOG_INFO("Recurse onInit: constructing application systems");

        auto& dispatcher = ctx.dispatcher;
        auto& timeline = ctx.timeline;
        auto& ecsWorld = ctx.world;

        // ECS core components
        ecsWorld.registerCoreComponents();
#ifdef FABRIC_ECS_INSPECTOR
        ecsWorld.enableInspector();
#endif

        // -- Key bindings ------------------------------------------------
        ctx.inputManager->bindKey("move_forward", SDLK_W);
        ctx.inputManager->bindKey("move_backward", SDLK_S);
        ctx.inputManager->bindKey("move_left", SDLK_A);
        ctx.inputManager->bindKey("move_right", SDLK_D);
        ctx.inputManager->bindKey("move_up", SDLK_SPACE);
        ctx.inputManager->bindKey("move_down", SDLK_LSHIFT);

        ctx.inputManager->bindKey("time_pause", SDLK_P);
        ctx.inputManager->bindKey("time_faster", SDLK_EQUALS);
        ctx.inputManager->bindKey("time_slower", SDLK_MINUS);

        ctx.inputManager->bindKey("toggle_fly", SDLK_F);
        ctx.inputManager->bindKey("toggle_debug", SDLK_F3);
        ctx.inputManager->bindKey("toggle_wireframe", SDLK_F4);
        ctx.inputManager->bindKey("toggle_camera", SDLK_V);
        ctx.inputManager->bindKey("toggle_collision_debug", SDLK_F10);
        ctx.inputManager->bindKey("toggle_bvh_debug", SDLK_F6);
        ctx.inputManager->bindKey("toggle_content_browser", SDLK_F7);
        ctx.inputManager->bindKey("toggle_bt_debug", SDLK_F11);
        ctx.inputManager->bindKey("cycle_bt_npc", SDLK_F8);

        // -- Timeline event listeners ------------------------------------
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

        // -- App Mode Manager observer -----------------------------------
        auto* window = ctx.window;
        ctx.appModeManager->addObserver([&timeline, window](fabric::AppMode, fabric::AppMode to) {
            const auto& modeFlags = fabric::AppModeManager::flags(to);
            SDL_SetWindowRelativeMouseMode(window, modeFlags.captureMouse);
            if (modeFlags.pauseSimulation) {
                timeline.pause();
            } else {
                timeline.resume();
            }
        });

        // Apply initial mode flags (no transition fires for the startup state)
        SDL_SetWindowRelativeMouseMode(ctx.window,
                                       fabric::AppModeManager::flags(ctx.appModeManager->current()).captureMouse);

        // -- Camera + CameraController -----------------------------------
        state->cameraCtrl = std::make_unique<recurse::CameraController>(*ctx.camera);

        // -- Terrain: density + essence fields, generator, cave carver ---
        recurse::TerrainConfig terrainConfig;
        terrainConfig.seed = 42;
        terrainConfig.frequency = 0.02f;
        terrainConfig.octaves = 4;
        state->terrainGen = std::make_unique<recurse::TerrainGenerator>(terrainConfig);

        recurse::CaveConfig caveConfig;
        caveConfig.seed = 42;
        state->caveCarver = std::make_unique<recurse::CaveCarver>(caveConfig);

        // -- Chunk mesh management (CPU side, budgeted re-meshing) -------
        state->meshManager =
            std::make_unique<recurse::ChunkMeshManager>(dispatcher, state->density.grid(), state->essence.grid());

        // -- Chunk streaming ---------------------------------------------
        recurse::StreamingConfig streamConfig;
        streamConfig.baseRadius = 3;
        streamConfig.maxRadius = 5;
        streamConfig.maxLoadsPerTick = 2;
        streamConfig.maxUnloadsPerTick = 4;
        state->streaming = std::make_unique<recurse::ChunkStreamingManager>(streamConfig);

        // -- Debug draw overlay ------------------------------------------
        state->debugDraw.init();

        // -- Physics (must precede VoxelChanged handler) -----------------
        state->physicsWorld.init(4096, 0);

        // Invalidate GPU mesh and physics collision when voxel data changes
        dispatcher.addEventListener(recurse::kVoxelChangedEvent, [state](fabric::Event& e) {
            int cx = e.getData<int>("cx");
            int cy = e.getData<int>("cy");
            int cz = e.getData<int>("cz");
            state->gpuUploadQueue.insert({cx, cy, cz});
            state->physicsWorld.rebuildChunkCollision(state->density.grid(), cx, cy, cz);
        });

        // -- Initial terrain generation + meshing ------------------------
        {
            FABRIC_ZONE_SCOPED_N("initial_terrain");
            auto initLoad = state->streaming->update(kSpawnX, kSpawnY, kSpawnZ, 0.0f);

            for (const auto& coord : initLoad.toLoad) {
                generateChunkTerrain(coord.cx, coord.cy, coord.cz, *state->terrainGen, *state->caveCarver,
                                     state->density, state->essence);
                state->meshManager->markDirty(coord.cx, coord.cy, coord.cz);

                auto ent = ecsWorld.get().entity().add<fabric::SceneEntity>().set<fabric::BoundingBox>(
                    {static_cast<float>(coord.cx * recurse::kChunkSize),
                     static_cast<float>(coord.cy * recurse::kChunkSize),
                     static_cast<float>(coord.cz * recurse::kChunkSize),
                     static_cast<float>((coord.cx + 1) * recurse::kChunkSize),
                     static_cast<float>((coord.cy + 1) * recurse::kChunkSize),
                     static_cast<float>((coord.cz + 1) * recurse::kChunkSize)});
                state->chunkEntities[coord] = ent;
            }

            // Flush dirty chunks for initial load with bounded passes
            constexpr int kMaxInitialRemeshPasses = 512;
            constexpr int kMaxInitialNoProgressPasses = 8;

            size_t previousDirty = std::numeric_limits<size_t>::max();
            int noProgressPasses = 0;
            int totalRemeshed = 0;

            for (int pass = 0; pass < kMaxInitialRemeshPasses; ++pass) {
                size_t dirtyBefore = state->meshManager->dirtyCount();
                if (dirtyBefore == 0)
                    break;

                int remeshed = state->meshManager->update();
                totalRemeshed += remeshed;

                size_t dirtyAfter = state->meshManager->dirtyCount();
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

            // Upload all ready initial meshes to GPU
            for (const auto& coord : initLoad.toLoad) {
                if (state->meshManager->isDirty(coord))
                    continue;

                const auto* data = state->meshManager->meshFor(coord);
                if (data && !data->vertices.empty()) {
                    state->gpuMeshes[coord] = uploadChunkMesh(*data);
                }
            }

            FABRIC_LOG_INFO(
                "Initial terrain: {} chunks loaded, {} remeshed, {} GPU meshes, {} chunks pending runtime remesh",
                initLoad.toLoad.size(), totalRemeshed, state->gpuMeshes.size(), state->meshManager->dirtyCount());
        }

        // -- Character systems -------------------------------------------
        constexpr float kCharWidth = 0.6f;
        constexpr float kCharHeight = 1.8f;
        constexpr float kCharDepth = 0.6f;

        state->charCtrl = std::make_unique<recurse::CharacterController>(kCharWidth, kCharHeight, kCharDepth);
        state->flightCtrl = std::make_unique<recurse::FlightController>(kCharWidth, kCharHeight, kCharDepth);

        // -- Voxel interaction -------------------------------------------
        state->voxelInteraction =
            std::make_unique<recurse::VoxelInteraction>(state->density, state->essence, dispatcher);

        // -- Shadow system -----------------------------------------------
        state->shadowSystem =
            std::make_unique<recurse::ShadowSystem>(recurse::presetConfig(recurse::ShadowQualityPreset::Medium));

        state->lightDir = fabric::Vec3f(0.5f, 0.8f, 0.3f);
        {
            float len = std::sqrt(state->lightDir.x * state->lightDir.x + state->lightDir.y * state->lightDir.y +
                                  state->lightDir.z * state->lightDir.z);
            state->lightDir = fabric::Vec3f(state->lightDir.x / len, state->lightDir.y / len, state->lightDir.z / len);
        }

        // -- OIT compositor ----------------------------------------------
        int pw, ph;
        SDL_GetWindowSizeInPixels(ctx.window, &pw, &ph);
        state->oitCompositor.init(static_cast<uint16_t>(pw), static_cast<uint16_t>(ph));

        // -- Ragdoll, AI, Audio ------------------------------------------
        state->ragdoll.init(&state->physicsWorld);

        state->audioSystem.setThreadedMode(true);
        state->audioSystem.init();
        state->audioSystem.setDensityGrid(&state->density.grid());

        state->behaviorAI.init(ecsWorld.get());

        state->pathfinding.init();

        state->animEvents.init();

        // -- Debug HUD ---------------------------------------------------
        state->debugHUD.init(ctx.rmlContext);

        // -- BT Debug Panel ----------------------------------------------
        state->btDebugPanel.init(ctx.rmlContext);

        // -- Content Browser ---------------------------------------------
        state->contentBrowser.init("assets/");

        // -- Developer Console -------------------------------------------
        state->devConsole.init(ctx.rmlContext);

        // Backtick toggles console via AppModeManager (Game <-> Console)
        auto* appMode = ctx.appModeManager;
        auto* inputRouter = ctx.inputRouter;
        ctx.inputRouter->setConsoleToggleCallback([state, appMode, inputRouter]() {
            auto mode = appMode->current();
            if (mode == fabric::AppMode::Console) {
                appMode->transition(fabric::AppMode::Game);
            } else if (mode == fabric::AppMode::Game) {
                appMode->transition(fabric::AppMode::Console);
            }
            state->devConsole.toggle();
            if (state->devConsole.isVisible()) {
                inputRouter->setMode(fabric::InputMode::UIOnly);
            } else {
                inputRouter->setMode(fabric::InputMode::GameOnly);
            }
        });

        // Console quit pushes SDL_QUIT (FabricApp::run() owns the running flag)
        state->devConsole.setQuitCallback([]() {
            SDL_Event qe{};
            qe.type = SDL_EVENT_QUIT;
            SDL_PushEvent(&qe);
        });

        // -- Save system + toast notifications ---------------------------
        state->saveManager = std::make_unique<recurse::SaveManager>("saves");

        // F5 = quicksave
        ctx.inputRouter->registerKeyCallback(SDLK_F5, [state, &ecsWorld, &timeline]() {
            recurse::SceneSerializer qsSerializer;
            if (state->saveManager->save("quicksave", qsSerializer, ecsWorld, state->density, state->essence, timeline,
                                         std::optional<fabric::Position>(fabric::Position{
                                             state->playerPos.x, state->playerPos.y, state->playerPos.z}),
                                         std::optional<fabric::Position>(fabric::Position{
                                             state->playerVel.x, state->playerVel.y, state->playerVel.z}))) {
                state->toastManager.show("Quick save complete", 2.0f);
                FABRIC_LOG_INFO("Quick save complete");
            } else {
                state->toastManager.show("Quick save failed", 3.0f);
                FABRIC_LOG_ERROR("Quick save failed");
            }
        });

        // F9 = quickload
        ctx.inputRouter->registerKeyCallback(SDLK_F9, [state, &ecsWorld, &timeline]() {
            recurse::SceneSerializer qlSerializer;
            std::optional<fabric::Position> loadedPos;
            std::optional<fabric::Position> loadedVel;
            if (state->saveManager->load("quicksave", qlSerializer, ecsWorld, state->density, state->essence, timeline,
                                         loadedPos, loadedVel)) {
                if (loadedPos) {
                    state->playerPos = fabric::Vec3f(loadedPos->x, loadedPos->y, loadedPos->z);
                }
                if (loadedVel) {
                    state->playerVel = recurse::Velocity{loadedVel->x, loadedVel->y, loadedVel->z};
                }
                state->toastManager.show("Quick load complete", 2.0f);
                FABRIC_LOG_INFO("Quick load complete");
            } else {
                state->toastManager.show("Quick load failed", 3.0f);
                FABRIC_LOG_ERROR("Quick load failed");
            }
        });

        // -- Particle system + DebrisPool emitter wiring -----------------
        state->particleSystem.init();

        state->debrisPool.enableParticleConversion(true);
        state->debrisPool.setParticleEmitter(
            [state](const fabric::Vector3<float, fabric::Space::World>& pos, float radius, int count) {
                state->particleSystem.emit(pos, radius, count, recurse::ParticleType::DebrisPuff);
            });

        // -- Toggle event handlers ---------------------------------------
        dispatcher.addEventListener("toggle_fly", [state](fabric::Event&) {
            if (state->movementFSM.isFlying()) {
                state->movementFSM.tryTransition(recurse::CharacterState::Falling);
                FABRIC_LOG_INFO("Flight mode: off");
            } else {
                state->movementFSM.tryTransition(recurse::CharacterState::Flying);
                state->playerVel = {};
                FABRIC_LOG_INFO("Flight mode: on");
            }
        });

        dispatcher.addEventListener("toggle_debug", [state](fabric::Event&) { state->debugHUD.toggle(); });

        dispatcher.addEventListener("toggle_wireframe", [state](fabric::Event&) {
            state->debugDraw.toggleWireframe();
            FABRIC_LOG_INFO("Wireframe: {}", state->debugDraw.isWireframeEnabled() ? "on" : "off");
        });

        dispatcher.addEventListener("toggle_camera", [state](fabric::Event&) {
            if (state->cameraCtrl->mode() == recurse::CameraMode::FirstPerson) {
                state->cameraCtrl->setMode(recurse::CameraMode::ThirdPerson);
            } else {
                state->cameraCtrl->setMode(recurse::CameraMode::FirstPerson);
            }
        });

        dispatcher.addEventListener("toggle_collision_debug", [state](fabric::Event&) {
            state->debugDraw.toggleFlag(recurse::DebugDrawFlags::CollisionShapes);
            FABRIC_LOG_INFO("Collision shapes: {}",
                            state->debugDraw.hasFlag(recurse::DebugDrawFlags::CollisionShapes) ? "on" : "off");
        });

        dispatcher.addEventListener("toggle_bvh_debug", [state](fabric::Event&) {
            state->debugDraw.toggleFlag(recurse::DebugDrawFlags::BVHOverlay);
            FABRIC_LOG_INFO("BVH overlay: {}",
                            state->debugDraw.hasFlag(recurse::DebugDrawFlags::BVHOverlay) ? "on" : "off");
        });

        dispatcher.addEventListener("toggle_content_browser", [state, appMode](fabric::Event&) {
            auto mode = appMode->current();
            if (mode == fabric::AppMode::Editor) {
                appMode->transition(fabric::AppMode::Game);
            } else if (mode == fabric::AppMode::Game) {
                appMode->transition(fabric::AppMode::Editor);
            }
            state->contentBrowser.toggle();
            FABRIC_LOG_INFO("Content Browser: {}", state->contentBrowser.isVisible() ? "on" : "off");
        });

        dispatcher.addEventListener("toggle_bt_debug", [state, appMode](fabric::Event&) {
            auto mode = appMode->current();
            if (mode == fabric::AppMode::Menu) {
                appMode->transition(fabric::AppMode::Game);
            } else if (mode == fabric::AppMode::Game) {
                appMode->transition(fabric::AppMode::Menu);
            }
            state->btDebugPanel.toggle();
            FABRIC_LOG_INFO("BT Debug: {}", state->btDebugPanel.isVisible() ? "on" : "off");
        });

        dispatcher.addEventListener("cycle_bt_npc", [state, &ecsWorld](fabric::Event&) {
            state->btDebugPanel.selectNextNPC(state->behaviorAI, ecsWorld.get());
            state->btDebugSelectedNpc = state->btDebugPanel.selectedNpc();
        });

        // Jump on space press (grounded only; in flight, move_up is continuous)
        dispatcher.addEventListener("move_up", [state](fabric::Event&) {
            if (state->movementFSM.isGrounded()) {
                state->movementFSM.tryTransition(recurse::CharacterState::Jumping);
                state->playerVel.y = state->charConfig.jumpForce;
            }
        });

        FABRIC_LOG_INFO("Recurse onInit complete");
    };

    // -----------------------------------------------------------------
    // onFixedUpdate: per-tick game simulation
    // -----------------------------------------------------------------
    desc.onFixedUpdate = [state](fabric::AppContext& ctx, float dt) {
        auto& ecsWorld = ctx.world;
        auto* inputManager = ctx.inputManager;

        // Mouse look (once per frame; subsequent ticks within the same
        // frame see zero delta since processMouseInput does not reset
        // the InputManager accumulator; beginFrame does that later)
        if (!state->mouseLookProcessed) {
            state->cameraCtrl->processMouseInput(inputManager->mouseDeltaX(), inputManager->mouseDeltaY());
            state->mouseLookProcessed = true;
        }

        // Autosave + toast
        state->saveManager->tickAutosave(
            dt, state->saveSerializer, ecsWorld, state->density, state->essence, ctx.timeline,
            std::optional<fabric::Position>(
                fabric::Position{state->playerPos.x, state->playerPos.y, state->playerPos.z}),
            std::optional<fabric::Position>(
                fabric::Position{state->playerVel.x, state->playerVel.y, state->playerVel.z}));
        state->toastManager.update(dt);

        // Streaming: load/unload chunks around player
        float speed = std::sqrt(state->playerVel.x * state->playerVel.x + state->playerVel.y * state->playerVel.y +
                                state->playerVel.z * state->playerVel.z);
        auto streamUpdate = state->streaming->update(state->playerPos.x, state->playerPos.y, state->playerPos.z, speed);

        for (const auto& coord : streamUpdate.toLoad) {
            generateChunkTerrain(coord.cx, coord.cy, coord.cz, *state->terrainGen, *state->caveCarver, state->density,
                                 state->essence);
            state->meshManager->markDirty(coord.cx, coord.cy, coord.cz);
            state->gpuUploadQueue.insert(coord);

            if (state->chunkEntities.find(coord) == state->chunkEntities.end()) {
                auto ent = ecsWorld.get().entity().add<fabric::SceneEntity>().set<fabric::BoundingBox>(
                    {static_cast<float>(coord.cx * recurse::kChunkSize),
                     static_cast<float>(coord.cy * recurse::kChunkSize),
                     static_cast<float>(coord.cz * recurse::kChunkSize),
                     static_cast<float>((coord.cx + 1) * recurse::kChunkSize),
                     static_cast<float>((coord.cy + 1) * recurse::kChunkSize),
                     static_cast<float>((coord.cz + 1) * recurse::kChunkSize)});
                state->chunkEntities[coord] = ent;
            }
        }
        for (const auto& coord : streamUpdate.toUnload) {
            state->gpuUploadQueue.erase(coord);
            state->meshManager->removeChunk(coord);
            state->physicsWorld.removeChunkCollision(coord.cx, coord.cy, coord.cz);

            if (auto it = state->chunkEntities.find(coord); it != state->chunkEntities.end()) {
                it->second.destruct();
                state->chunkEntities.erase(it);
            }
            if (auto it = state->gpuMeshes.find(coord); it != state->gpuMeshes.end()) {
                recurse::VoxelMesher::destroyMesh(it->second);
                state->gpuMeshes.erase(it);
            }
            state->density.grid().removeChunk(coord.cx, coord.cy, coord.cz);
            state->essence.grid().removeChunk(coord.cx, coord.cy, coord.cz);
        }

        // Character movement
        if (state->movementFSM.isFlying()) {
            auto fwd = state->cameraCtrl->forward();
            auto right = state->cameraCtrl->right();

            fabric::Vec3f moveDir(0.0f, 0.0f, 0.0f);
            if (inputManager->isActionActive("move_forward"))
                moveDir = moveDir + fwd;
            if (inputManager->isActionActive("move_backward"))
                moveDir = moveDir - fwd;
            if (inputManager->isActionActive("move_right"))
                moveDir = moveDir + right;
            if (inputManager->isActionActive("move_left"))
                moveDir = moveDir - right;
            if (inputManager->isActionActive("move_up"))
                moveDir = moveDir + fabric::Vec3f(0.0f, 1.0f, 0.0f);
            if (inputManager->isActionActive("move_down"))
                moveDir = moveDir - fabric::Vec3f(0.0f, 1.0f, 0.0f);

            float len = std::sqrt(moveDir.x * moveDir.x + moveDir.y * moveDir.y + moveDir.z * moveDir.z);
            if (len > 0.001f)
                moveDir = fabric::Vec3f(moveDir.x / len, moveDir.y / len, moveDir.z / len);

            fabric::Vec3f displacement(moveDir.x * state->charConfig.flightSpeed * dt,
                                       moveDir.y * state->charConfig.flightSpeed * dt,
                                       moveDir.z * state->charConfig.flightSpeed * dt);

            auto result = state->flightCtrl->move(state->playerPos, displacement, state->density.grid());
            state->playerPos = result.resolvedPosition;

        } else {
            // Ground mode: flatten forward/right to XZ plane
            auto fwd = state->cameraCtrl->forward();
            auto right = state->cameraCtrl->right();

            fabric::Vec3f flatFwd(fwd.x, 0.0f, fwd.z);
            float fwdLen = std::sqrt(flatFwd.x * flatFwd.x + flatFwd.z * flatFwd.z);
            if (fwdLen > 0.001f)
                flatFwd = fabric::Vec3f(flatFwd.x / fwdLen, 0.0f, flatFwd.z / fwdLen);

            fabric::Vec3f flatRight(right.x, 0.0f, right.z);
            float rightLen = std::sqrt(flatRight.x * flatRight.x + flatRight.z * flatRight.z);
            if (rightLen > 0.001f)
                flatRight = fabric::Vec3f(flatRight.x / rightLen, 0.0f, flatRight.z / rightLen);

            fabric::Vec3f horizMove(0.0f, 0.0f, 0.0f);
            if (inputManager->isActionActive("move_forward"))
                horizMove = horizMove + flatFwd;
            if (inputManager->isActionActive("move_backward"))
                horizMove = horizMove - flatFwd;
            if (inputManager->isActionActive("move_right"))
                horizMove = horizMove + flatRight;
            if (inputManager->isActionActive("move_left"))
                horizMove = horizMove - flatRight;

            float horizLen = std::sqrt(horizMove.x * horizMove.x + horizMove.z * horizMove.z);
            if (horizLen > 0.001f)
                horizMove = fabric::Vec3f(horizMove.x / horizLen, 0.0f, horizMove.z / horizLen);

            // Gravity
            state->playerVel.y -= state->charConfig.gravity * dt;

            fabric::Vec3f displacement(horizMove.x * state->charConfig.walkSpeed * dt, state->playerVel.y * dt,
                                       horizMove.z * state->charConfig.walkSpeed * dt);

            auto result = state->charCtrl->move(state->playerPos, displacement, state->density.grid());
            state->playerPos = result.resolvedPosition;

            if (result.onGround) {
                state->playerVel.y = 0.0f;
                if (state->movementFSM.currentState() == recurse::CharacterState::Falling ||
                    state->movementFSM.currentState() == recurse::CharacterState::Jumping) {
                    state->movementFSM.tryTransition(recurse::CharacterState::Grounded);
                }
            } else if (state->movementFSM.isGrounded()) {
                state->movementFSM.tryTransition(recurse::CharacterState::Falling);
            }

            // Ceiling collision: kill upward velocity
            if (result.hitY && state->playerVel.y > 0.0f)
                state->playerVel.y = 0.0f;
        }

        // Voxel interaction (mouse buttons)
        state->interactionCooldown -= dt;
        if (state->interactionCooldown <= 0.0f) {
            auto camPos = state->cameraCtrl->position();
            auto camFwd = state->cameraCtrl->forward();

            if (inputManager->mouseButton(1)) {
                auto r = state->voxelInteraction->destroyMatterAt(state->density.grid(), camPos.x, camPos.y, camPos.z,
                                                                  camFwd.x, camFwd.y, camFwd.z, 10.0f);
                if (r.success)
                    state->interactionCooldown = kInteractionRate;
            }
            if (inputManager->mouseButton(3)) {
                auto r = state->voxelInteraction->createMatterAt(
                    state->density.grid(), camPos.x, camPos.y, camPos.z, camFwd.x, camFwd.y, camFwd.z, 1.0f,
                    fabric::Vector4<float, fabric::Space::World>(0.4f, 0.7f, 0.3f, 1.0f), 10.0f);
                if (r.success)
                    state->interactionCooldown = kInteractionRate;
            }
        }

        // Physics and AI step at fixed rate
        {
            FABRIC_ZONE_SCOPED_N("physics_step");
            state->physicsWorld.step(dt);
            state->behaviorAI.update(dt);
        }

        // LOD: compute per-chunk LOD from camera distance (marks dirty on change)
        {
            auto camPos = state->cameraCtrl->position();
            state->meshManager->updateLOD(camPos.x, camPos.y, camPos.z);
        }

        // Mesh manager: budgeted CPU re-meshing of dirty chunks
        state->meshManager->update();

        // Particle simulation + debris-to-particle conversion
        state->debrisPool.update(dt);
        state->particleSystem.update(dt);

        // GPU mesh sync: upload re-meshed chunks
        {
            FABRIC_ZONE_SCOPED_N("chunk_mesh_upload");
            auto it = state->gpuUploadQueue.begin();
            while (it != state->gpuUploadQueue.end()) {
                if (state->chunkEntities.find(*it) == state->chunkEntities.end()) {
                    it = state->gpuUploadQueue.erase(it);
                    continue;
                }

                if (!state->meshManager->isDirty(*it)) {
                    const auto* data = state->meshManager->meshFor(*it);
                    if (auto git = state->gpuMeshes.find(*it); git != state->gpuMeshes.end()) {
                        recurse::VoxelMesher::destroyMesh(git->second);
                        state->gpuMeshes.erase(git);
                    }
                    if (data && !data->vertices.empty()) {
                        state->gpuMeshes[*it] = uploadChunkMesh(*data);
                    }
                    it = state->gpuUploadQueue.erase(it);
                } else {
                    ++it;
                }
            }
        }
    };

    // -----------------------------------------------------------------
    // onRender: per-frame render logic
    // -----------------------------------------------------------------
    desc.onRender = [state](fabric::AppContext& ctx, float frameTime) {
        ++state->frameCounter;

        // Reset mouse look flag for the next frame's fixed update
        state->mouseLookProcessed = false;

        auto* camera = ctx.camera;
        auto* sceneView = ctx.sceneView;
        auto* window = ctx.window;

        // Camera tracks player position (spring arm collision for 3P mode)
        state->cameraCtrl->update(state->playerPos, frameTime, &state->density.grid());

        // Audio listener follows camera
        {
            FABRIC_ZONE_SCOPED_N("audio_update");
            state->audioSystem.setListenerPosition(state->cameraCtrl->position());
            state->audioSystem.setListenerDirection(state->cameraCtrl->forward(), state->cameraCtrl->up());
            state->audioSystem.update(frameTime);
        }

        // Shadow cascade computation
        state->shadowSystem->update(*camera, fabric::Vector3<float, fabric::Space::World>(
                                                 state->lightDir.x, state->lightDir.y, state->lightDir.z));
        state->voxelRenderer.setLightDirection(
            fabric::Vector3<float, fabric::Space::World>(state->lightDir.x, state->lightDir.y, state->lightDir.z));

        // -- Render submit -----------------------------------------------
        {
            FABRIC_ZONE_SCOPED_N("render_submit");

            // Apply debug overlay state (wireframe toggle)
            state->debugDraw.applyDebugFlags();

            // ECS entity rendering (SceneView: cull, build render list, submit)
            sceneView->render();

            std::unordered_set<flecs::entity_t> visibleEntityIds;
            visibleEntityIds.reserve(sceneView->visibleEntities().size());
            for (const auto& entity : sceneView->visibleEntities()) {
                visibleEntityIds.insert(entity.id());
            }

            // Voxel chunk rendering (frustum-filtered via chunk entities)
            for (const auto& [coord, mesh] : state->gpuMeshes) {
                auto entIt = state->chunkEntities.find(coord);
                if (entIt == state->chunkEntities.end())
                    continue;
                if (visibleEntityIds.find(entIt->second.id()) == visibleEntityIds.end())
                    continue;
                state->voxelRenderer.render(sceneView->geometryViewId(), mesh, coord.cx, coord.cy, coord.cz);
            }

            // OIT accumulation pass (weighted blended transparency)
            if (state->oitCompositor.isValid() && !sceneView->transparentEntities().empty()) {
                int oitPW, oitPH;
                SDL_GetWindowSizeInPixels(window, &oitPW, &oitPH);
                state->oitCompositor.beginAccumulation(fabric::kOITAccumViewId, camera->viewMatrix(),
                                                       camera->projectionMatrix(), static_cast<uint16_t>(oitPW),
                                                       static_cast<uint16_t>(oitPH));
                state->oitCompositor.composite(fabric::kOITCompositeViewId, static_cast<uint16_t>(oitPW),
                                               static_cast<uint16_t>(oitPH));
            }

            // Particle billboard rendering (dedicated view, alpha blended)
            {
                int curPW, curPH;
                SDL_GetWindowSizeInPixels(window, &curPW, &curPH);
                state->particleSystem.render(camera->viewMatrix(), camera->projectionMatrix(),
                                             static_cast<uint16_t>(curPW), static_cast<uint16_t>(curPH));
            }

            // Debug draw overlay (lines, shapes) on geometry view
            state->debugDraw.begin(sceneView->geometryViewId());

            // Collision shape overlays (F10)
            if (state->debugDraw.hasFlag(recurse::DebugDrawFlags::CollisionShapes)) {
                state->debugDraw.setColor(0xff00ff00); // green (ABGR)
                for (const auto& [coord, ent] : state->chunkEntities) {
                    if (state->physicsWorld.chunkCollisionShapeCount(coord.cx, coord.cy, coord.cz) > 0) {
                        float x0 = static_cast<float>(coord.cx * recurse::kChunkSize);
                        float y0 = static_cast<float>(coord.cy * recurse::kChunkSize);
                        float z0 = static_cast<float>(coord.cz * recurse::kChunkSize);
                        float x1 = x0 + static_cast<float>(recurse::kChunkSize);
                        float y1 = y0 + static_cast<float>(recurse::kChunkSize);
                        float z1 = z0 + static_cast<float>(recurse::kChunkSize);
                        state->debugDraw.drawWireBox(x0, y0, z0, x1, y1, z1);
                    }
                }
            }

            // BVH overlay with depth coloring (F6)
            if (state->debugDraw.hasFlag(recurse::DebugDrawFlags::BVHOverlay)) {
                fabric::BVH<int> chunkBVH;
                int idx = 0;
                for (const auto& [coord, ent] : state->chunkEntities) {
                    float x0 = static_cast<float>(coord.cx * recurse::kChunkSize);
                    float y0 = static_cast<float>(coord.cy * recurse::kChunkSize);
                    float z0 = static_cast<float>(coord.cz * recurse::kChunkSize);
                    float x1 = x0 + static_cast<float>(recurse::kChunkSize);
                    float y1 = y0 + static_cast<float>(recurse::kChunkSize);
                    float z1 = z0 + static_cast<float>(recurse::kChunkSize);
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
                    state->debugDraw.setColor(color);
                    state->debugDraw.drawWireBox(bounds.min.x, bounds.min.y, bounds.min.z, bounds.max.x, bounds.max.y,
                                                 bounds.max.z);
                });
            }

            state->debugDraw.end();
        }

        // Debug HUD data update
        {
            fabric::DebugData debugData;
            debugData.fps = (frameTime > 0.0f) ? 1.0f / frameTime : 0.0f;
            debugData.frameTimeMs = frameTime * 1000.0f;
            debugData.visibleChunks = static_cast<int>(state->gpuMeshes.size());
            debugData.totalChunks = static_cast<int>(state->density.grid().chunkCount());
            debugData.cameraPosition = state->cameraCtrl->position();
            debugData.currentRadius = state->streaming->currentRadius();
            debugData.currentState = recurse::MovementFSM::stateToString(state->movementFSM.currentState());

            int triCount = 0;
            for (const auto& [_, m] : state->gpuMeshes)
                triCount += static_cast<int>(m.indexCount / 3);
            debugData.triangleCount = triCount;

            // Perf overlay: bgfx stats (returns previous frame data)
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
            if (auto* jolt = state->physicsWorld.joltSystem()) {
                debugData.physicsBodyCount = static_cast<int>(jolt->GetNumBodies());
            }
            debugData.audioVoiceCount = static_cast<int>(state->audioSystem.activeSoundCount());
            debugData.chunkMeshQueueSize = static_cast<int>(state->meshManager->dirtyCount());

            state->debugHUD.update(debugData);

            // Periodic performance log (every 10 frames)
            if (state->frameCounter % 10 == 0) {
                FABRIC_LOG_INFO("perf: frame={} fps={:.1f} dt={:.1f}ms gpu={:.1f}ms draws={} tris={} chunks={}/{} "
                                "meshQueue={} vram={:.1f}MB bodies={} voices={}",
                                state->frameCounter, debugData.fps, debugData.frameTimeMs, debugData.gpuTimeMs,
                                debugData.drawCallCount, debugData.triangleCount, debugData.visibleChunks,
                                debugData.totalChunks, debugData.chunkMeshQueueSize, debugData.memoryUsageMB,
                                debugData.physicsBodyCount, debugData.audioVoiceCount);
            }
        }

        if (state->btDebugPanel.isVisible()) {
            state->btDebugPanel.update(state->behaviorAI, state->btDebugSelectedNpc);
        }
    };

    // -----------------------------------------------------------------
    // onResize: recreate resolution-dependent resources
    // -----------------------------------------------------------------
    desc.onResize = [state](fabric::AppContext&, uint32_t width, uint32_t height) {
        if (state->oitCompositor.isValid()) {
            state->oitCompositor.shutdown();
            state->oitCompositor.init(static_cast<uint16_t>(width), static_cast<uint16_t>(height));
        }
    };

    // -----------------------------------------------------------------
    // onShutdown: tear down game systems in reverse init order
    // -----------------------------------------------------------------
    desc.onShutdown = [state](fabric::AppContext& ctx) {
        FABRIC_LOG_INFO("Recurse onShutdown: tearing down application systems");

        state->devConsole.shutdown();
        state->contentBrowser.shutdown();
        state->btDebugPanel.shutdown();
        state->debugHUD.shutdown();

        state->animEvents.shutdown();
        state->pathfinding.shutdown();
        state->behaviorAI.shutdown();
        state->audioSystem.shutdown();
        state->ragdoll.shutdown();
        state->physicsWorld.shutdown();

        // Destroy GPU meshes
        for (auto& [_, mesh] : state->gpuMeshes) {
            recurse::VoxelMesher::destroyMesh(mesh);
        }
        state->gpuMeshes.clear();

        // Destroy chunk ECS entities
        for (auto& [_, entity] : state->chunkEntities) {
            entity.destruct();
        }
        state->chunkEntities.clear();

        state->oitCompositor.shutdown();
        state->particleSystem.shutdown();
        state->voxelRenderer.shutdown();
        ctx.sceneView->skyRenderer().shutdown();
        state->debugDraw.shutdown();

        FABRIC_LOG_INFO("Recurse onShutdown complete");
    };

    return desc;
}

int main(int argc, char* argv[]) {
    fabric::log::init();
    int result = fabric::FabricApp::run(argc, argv, buildRecurseDesc());
    fabric::log::shutdown();
    return result;
}
