#include "recurse/systems/DebugOverlaySystem.hh"

#include "fabric/core/AppContext.hh"
#include "fabric/core/AppModeManager.hh"
#include "fabric/core/Camera.hh"
#include "fabric/core/Event.hh"
#include "fabric/core/InputRouter.hh"
#include "fabric/core/Log.hh"
#include "fabric/core/SceneView.hh"
#include "fabric/core/SystemRegistry.hh"
#include "fabric/simulation/ChunkActivityTracker.hh"
#include "fabric/simulation/SimulationGrid.hh"
#include "fabric/utils/BVH.hh"
#include "fabric/utils/Profiler.hh"
#include "recurse/ai/BehaviorAI.hh"
#include "recurse/gameplay/MovementFSM.hh"
#include "recurse/physics/PhysicsWorld.hh"
#include "recurse/systems/AIGameSystem.hh"
#include "recurse/systems/AudioGameSystem.hh"
#include "recurse/systems/CameraGameSystem.hh"
#include "recurse/systems/CharacterMovementSystem.hh"
#include "recurse/systems/ChunkPipelineSystem.hh"
#include "recurse/systems/OITRenderSystem.hh"
#include "recurse/systems/PhysicsGameSystem.hh"
#include "recurse/systems/TerrainSystem.hh"
#include "recurse/systems/VoxelMeshingSystem.hh"
#include "recurse/systems/VoxelRenderSystem.hh"
#include "recurse/systems/VoxelSimulationSystem.hh"
#include "recurse/world/ChunkStreaming.hh"
#include "recurse/world/SmoothVoxelVertex.hh"
#include "recurse/world/VoxelRaycast.hh"

#include <bgfx/bgfx.h>
#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>

namespace recurse::systems {

void DebugOverlaySystem::init(fabric::AppContext& ctx) {
    // Cache sibling system pointers
    camera_ = ctx.systemRegistry.get<CameraGameSystem>();
    chunks_ = ctx.systemRegistry.get<ChunkPipelineSystem>();
    physics_ = ctx.systemRegistry.get<PhysicsGameSystem>();
    audio_ = ctx.systemRegistry.get<AudioGameSystem>();
    ai_ = ctx.systemRegistry.get<AIGameSystem>();
    terrain_ = ctx.systemRegistry.get<TerrainSystem>();
    charMovement_ = ctx.systemRegistry.get<CharacterMovementSystem>();
    meshSystem_ = ctx.systemRegistry.get<VoxelMeshingSystem>();
    voxelSim_ = ctx.systemRegistry.get<VoxelSimulationSystem>();
    voxelSim_ = ctx.systemRegistry.get<VoxelSimulationSystem>();

    debugDraw_.init();
    debugHUD_.init(ctx.rmlContext);
    chunkDebugPanel_.init(ctx.rmlContext);
    wailaPanel_.init(ctx.rmlContext);
    hotkeyPanel_.init(ctx.rmlContext);
    btDebugPanel_.init(ctx.rmlContext);
    contentBrowser_.init("assets/");
    devConsole_.init(ctx.rmlContext);

    // Console toggle via InputRouter (backtick key)
    auto* appMode = ctx.appModeManager;
    auto* inputRouter = ctx.inputRouter;
    inputRouter->setConsoleToggleCallback([this, appMode]() {
        auto mode = appMode->current();
        if (mode == fabric::AppMode::Console) {
            appMode->transition(fabric::AppMode::Game);
        } else if (mode == fabric::AppMode::Game) {
            appMode->transition(fabric::AppMode::Console);
        }
        devConsole_.toggle();
    });

    // Console quit pushes SDL_QUIT
    devConsole_.setQuitCallback([]() {
        SDL_Event qe{};
        qe.type = SDL_EVENT_QUIT;
        SDL_PushEvent(&qe);
    });

    // Event listeners for debug toggles
    auto& dispatcher = ctx.dispatcher;

    dispatcher.addEventListener("toggle_debug", [this](fabric::Event&) {
        debugHUD_.toggle();
        // WAILA and HotkeyPanel are always visible now
    });

    dispatcher.addEventListener("toggle_chunk_debug", [this](fabric::Event&) { chunkDebugPanel_.toggle(); });

    dispatcher.addEventListener("toggle_wireframe", [this](fabric::Event&) {
        debugDraw_.toggleWireframe();
        FABRIC_LOG_INFO("Wireframe: {}", debugDraw_.isWireframeEnabled() ? "on" : "off");
    });

    dispatcher.addEventListener("toggle_collision_debug", [this](fabric::Event&) {
        debugDraw_.toggleFlag(recurse::DebugDrawFlags::CollisionShapes);
        FABRIC_LOG_INFO("Collision shapes: {}",
                        debugDraw_.hasFlag(recurse::DebugDrawFlags::CollisionShapes) ? "on" : "off");
    });

    dispatcher.addEventListener("toggle_bvh_debug", [this](fabric::Event&) {
        debugDraw_.toggleFlag(recurse::DebugDrawFlags::BVHOverlay);
        FABRIC_LOG_INFO("BVH overlay: {}", debugDraw_.hasFlag(recurse::DebugDrawFlags::BVHOverlay) ? "on" : "off");
    });

    dispatcher.addEventListener("toggle_chunk_states", [this](fabric::Event&) {
        debugDraw_.toggleFlag(recurse::DebugDrawFlags::ChunkStates);
        FABRIC_LOG_INFO("Chunk states: {}", debugDraw_.hasFlag(recurse::DebugDrawFlags::ChunkStates) ? "on" : "off");
    });

    dispatcher.addEventListener("toggle_content_browser", [this, appMode](fabric::Event&) {
        auto mode = appMode->current();
        if (mode == fabric::AppMode::Editor) {
            appMode->transition(fabric::AppMode::Game);
        } else if (mode == fabric::AppMode::Game) {
            appMode->transition(fabric::AppMode::Editor);
        }
        contentBrowser_.toggle();
        FABRIC_LOG_INFO("Content Browser: {}", contentBrowser_.isVisible() ? "on" : "off");
    });

    dispatcher.addEventListener("toggle_bt_debug", [this, appMode](fabric::Event&) {
        auto mode = appMode->current();
        if (mode == fabric::AppMode::Menu) {
            appMode->transition(fabric::AppMode::Game);
        } else if (mode == fabric::AppMode::Game) {
            appMode->transition(fabric::AppMode::Menu);
        }
        btDebugPanel_.toggle();
        FABRIC_LOG_INFO("BT Debug: {}", btDebugPanel_.isVisible() ? "on" : "off");
    });

    dispatcher.addEventListener("cycle_bt_npc", [this, &ctx](fabric::Event&) {
        btDebugPanel_.selectNextNPC(ai_->behaviorAI(), ctx.world.get());
        btDebugSelectedNpc_ = btDebugPanel_.selectedNpc();
    });

    // AppMode observer: update hotkey panel and WAILA visibility
    appMode->addObserver([this](fabric::AppMode /*from*/, fabric::AppMode to) {
        hotkeyPanel_.setMode(to);
        wailaPanel_.setMode(to);
    });

    // Initialize panels with current mode (observer only fires on transitions)
    hotkeyPanel_.setMode(appMode->current());
    wailaPanel_.setMode(appMode->current());

    FABRIC_LOG_INFO("DebugOverlaySystem initialized");
}

void DebugOverlaySystem::render(fabric::AppContext& ctx) {
    ++frameCounter_;

    auto* camera = ctx.camera;
    auto* sceneView = ctx.sceneView;

    // Apply wireframe toggle before debug line drawing
    debugDraw_.applyDebugFlags();

    // Debug draw overlay (lines, shapes) on geometry view
    debugDraw_.begin(sceneView->geometryViewId());

    // Collision shape overlays (F10)
    if (debugDraw_.hasFlag(recurse::DebugDrawFlags::CollisionShapes)) {
        debugDraw_.setColor(0xff00ff00); // green (ABGR)
        for (const auto& [coord, ent] : chunks_->chunkEntities()) {
            if (physics_->physicsWorld().chunkCollisionShapeCount(coord.cx, coord.cy, coord.cz) > 0) {
                float x0 = static_cast<float>(coord.cx * recurse::kChunkSize);
                float y0 = static_cast<float>(coord.cy * recurse::kChunkSize);
                float z0 = static_cast<float>(coord.cz * recurse::kChunkSize);
                float x1 = x0 + static_cast<float>(recurse::kChunkSize);
                float y1 = y0 + static_cast<float>(recurse::kChunkSize);
                float z1 = z0 + static_cast<float>(recurse::kChunkSize);
                debugDraw_.drawWireBox(x0, y0, z0, x1, y1, z1);
            }
        }
    }

    // BVH overlay with depth coloring (F6)
    if (debugDraw_.hasFlag(recurse::DebugDrawFlags::BVHOverlay)) {
        fabric::BVH<int> chunkBVH;
        int idx = 0;
        for (const auto& [coord, ent] : chunks_->chunkEntities()) {
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
            debugDraw_.setColor(color);
            debugDraw_.drawWireBox(bounds.min.x, bounds.min.y, bounds.min.z, bounds.max.x, bounds.max.y, bounds.max.z);
        });
    }

    // Chunk state visualization (F12)
    if (debugDraw_.hasFlag(recurse::DebugDrawFlags::ChunkStates) && voxelSim_) {
        auto& tracker = voxelSim_->activityTracker();
        auto& grid = voxelSim_->simulationGrid();

        for (auto [cx, cy, cz] : grid.allChunks()) {
            fabric::simulation::ChunkPos pos{cx, cy, cz};
            auto state = tracker.getState(pos);

            // State-based colors (ABGR format)
            uint32_t color;
            switch (state) {
                case fabric::simulation::ChunkState::Sleeping:
                    color = 0x80666680; // Semi-transparent gray-blue
                    break;
                case fabric::simulation::ChunkState::Active:
                    color = 0xcc4de666; // Bright green
                    break;
                case fabric::simulation::ChunkState::BoundaryDirty:
                    color = 0xb3cc33ff; // Yellow-orange
                    break;
                default:
                    color = 0x80808080; // Gray
                    break;
            }

            float x0 = static_cast<float>(cx * recurse::kChunkSize);
            float y0 = static_cast<float>(cy * recurse::kChunkSize);
            float z0 = static_cast<float>(cz * recurse::kChunkSize);
            float x1 = x0 + static_cast<float>(recurse::kChunkSize);
            float y1 = y0 + static_cast<float>(recurse::kChunkSize);
            float z1 = z0 + static_cast<float>(recurse::kChunkSize);

            debugDraw_.setColor(color);
            debugDraw_.drawWireBox(x0, y0, z0, x1, y1, z1);
        }
    }

    debugDraw_.end();

    // Debug HUD data collection
    {
        // render() receives no dt; derive frame timing from bgfx CPU stats
        const bgfx::Stats* stats = bgfx::getStats();

        float cpuMs = 0.0f;
        if (stats->cpuTimerFreq > 0) {
            cpuMs = 1000.0f * static_cast<float>(static_cast<double>(stats->cpuTimeEnd - stats->cpuTimeBegin) /
                                                 static_cast<double>(stats->cpuTimerFreq));
        }

        fabric::DebugData debugData;
        debugData.fps = (cpuMs > 0.0f) ? 1000.0f / cpuMs : 0.0f;
        debugData.frameTimeMs = cpuMs;
        debugData.visibleChunks = meshSystem_ ? static_cast<int>(meshSystem_->gpuMeshes().size()) : 0;
        debugData.totalChunks = static_cast<int>(terrain_->densityGrid().chunkCount());
        debugData.cameraPosition = camera_->position();
        debugData.currentRadius = chunks_->streaming().currentRadius();
        debugData.currentState = recurse::MovementFSM::stateToString(charMovement_->movementFSM().currentState());

        // Calculate total triangle count from GPU mesh index counts
        int triangleCount = 0;
        if (meshSystem_) {
            for (const auto& [coord, gpuMesh] : meshSystem_->gpuMeshes()) {
                triangleCount += static_cast<int>(gpuMesh.indexCount / 3);
            }
        }
        debugData.triangleCount = triangleCount;

        debugData.drawCallCount = static_cast<int>(stats->numDraw);
        if (stats->gpuTimerFreq > 0) {
            double gpuMs = 1000.0 * static_cast<double>(stats->gpuTimeEnd - stats->gpuTimeBegin) /
                           static_cast<double>(stats->gpuTimerFreq);
            if (gpuMs < 0.0 || gpuMs > 1000.0)
                gpuMs = 0.0;
            debugData.gpuTimeMs = static_cast<float>(gpuMs);
        }
        debugData.memoryUsageMB =
            static_cast<float>(stats->textureMemoryUsed + stats->rtMemoryUsed) / (1024.0f * 1024.0f);

        if (auto* jolt = physics_->physicsWorld().joltSystem()) {
            debugData.physicsBodyCount = static_cast<int>(jolt->GetNumBodies());
        }
        debugData.audioVoiceCount = static_cast<int>(audio_->activeSoundCount());
        debugData.chunkMeshQueueSize = 0; // No accessor available; would need VoxelMeshingSystem::pendingQueueSize()

        debugHUD_.update(debugData);

        // Periodic performance log (every 1000 frames)
        if (frameCounter_ % 1000 == 0) {
            FABRIC_LOG_INFO("perf: frame={} fps={:.1f} dt={:.1f}ms gpu={:.1f}ms draws={} tris={} chunks={}/{} "
                            "meshQueue={} vram={:.1f}MB bodies={} voices={}",
                            frameCounter_, debugData.fps, debugData.frameTimeMs, debugData.gpuTimeMs,
                            debugData.drawCallCount, debugData.triangleCount, debugData.visibleChunks,
                            debugData.totalChunks, debugData.chunkMeshQueueSize, debugData.memoryUsageMB,
                            debugData.physicsBodyCount, debugData.audioVoiceCount);
        }
    }

    // WAILA crosshair raycast (every frame - always visible)
    {
        fabric::WAILAData waila;
        auto camPos = camera_->position();
        auto camFwd = camera_->forward();
        auto hit = recurse::castRay(terrain_->densityGrid(), camPos.x, camPos.y, camPos.z, camFwd.x, camFwd.y, camFwd.z,
                                    20.0f);
        if (hit.has_value()) {
            waila.hasHit = true;
            waila.voxelX = hit->x;
            waila.voxelY = hit->y;
            waila.voxelZ = hit->z;
            waila.chunkX = hit->x >> recurse::kChunkShift;
            waila.chunkY = hit->y >> recurse::kChunkShift;
            waila.chunkZ = hit->z >> recurse::kChunkShift;
            waila.normalX = hit->nx;
            waila.normalY = hit->ny;
            waila.normalZ = hit->nz;
            waila.distance = hit->t;
            waila.density = terrain_->densityGrid().get(hit->x, hit->y, hit->z);
            auto ess = terrain_->essenceGrid().get(hit->x, hit->y, hit->z);
            waila.essenceR = ess.x;
            waila.essenceG = ess.y;
            waila.essenceB = ess.z;
        }
        wailaPanel_.update(waila);
    }

    // BT debug panel
    if (btDebugPanel_.isVisible()) {
        btDebugPanel_.update(ai_->behaviorAI(), btDebugSelectedNpc_);
    }

    // Chunk debug panel
    if (chunkDebugPanel_.isVisible() && meshSystem_) {
        fabric::ChunkDebugData chunkData;
        chunkData.activeChunks = meshSystem_->gpuMeshCount();
        chunkData.gpuMeshCount = meshSystem_->gpuMeshCount();
        chunkData.dirtyChunksPending = meshSystem_->pendingMeshCount();
        chunkData.vertexCount = meshSystem_->vertexBufferSize();
        chunkData.indexCount = meshSystem_->indexBufferSize();

        // Calculate buffer sizes in MB (vertex: 36 bytes, index: 4 bytes)
        constexpr float kVertexSize = sizeof(recurse::SmoothVoxelVertex);
        constexpr float kIndexSize = sizeof(uint32_t);
        constexpr float kMB = 1024.0f * 1024.0f;
        chunkData.vertexBufferMB = static_cast<float>(chunkData.vertexCount) * kVertexSize / kMB;
        chunkData.indexBufferMB = static_cast<float>(chunkData.indexCount) * kIndexSize / kMB;

        chunkDebugPanel_.update(chunkData);
    }
}

void DebugOverlaySystem::shutdown() {
    devConsole_.shutdown();
    contentBrowser_.shutdown();
    btDebugPanel_.shutdown();
    chunkDebugPanel_.shutdown();
    hotkeyPanel_.shutdown();
    wailaPanel_.shutdown();
    debugHUD_.shutdown();
    debugDraw_.shutdown();
    FABRIC_LOG_INFO("DebugOverlaySystem shut down");
}

void DebugOverlaySystem::configureDependencies() {
    // Run after opaque + OIT rendering so debug lines appear on top
    after<VoxelRenderSystem>();
    after<OITRenderSystem>();
    after<CameraGameSystem>();
    after<ChunkPipelineSystem>();
    after<PhysicsGameSystem>();
    after<AudioGameSystem>();
    after<AIGameSystem>();
    after<TerrainSystem>();
    after<CharacterMovementSystem>();
    after<VoxelSimulationSystem>();
}

} // namespace recurse::systems
