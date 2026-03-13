#include "recurse/systems/DebugOverlaySystem.hh"

#include "fabric/core/AppContext.hh"
#include "fabric/core/AppModeManager.hh"
#include "fabric/core/Event.hh"
#include "fabric/core/Log.hh"
#include "fabric/core/SystemRegistry.hh"
#include "fabric/input/InputRouter.hh"
#include "fabric/platform/JobScheduler.hh"
#include "fabric/render/Camera.hh"
#include "fabric/render/SceneView.hh"
#include "fabric/utils/BVH.hh"
#include "fabric/utils/Profiler.hh"
#include "recurse/ai/BehaviorAI.hh"
#include "recurse/character/MovementFSM.hh"
#include "recurse/physics/PhysicsWorld.hh"
#include "recurse/simulation/ChunkActivityTracker.hh"
#include "recurse/simulation/ChunkRegistry.hh"
#include "recurse/simulation/MaterialRegistry.hh"
#include "recurse/simulation/SimulationGrid.hh"
#include "recurse/systems/AIGameSystem.hh"
#include "recurse/systems/AudioGameSystem.hh"
#include "recurse/systems/CameraGameSystem.hh"
#include "recurse/systems/CharacterMovementSystem.hh"
#include "recurse/systems/ChunkPipelineSystem.hh"
#include "recurse/systems/LODSystem.hh"
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

void DebugOverlaySystem::doInit(fabric::AppContext& ctx) {
    // Cache sibling system pointers
    camera_ = ctx.systemRegistry.get<CameraGameSystem>();
    chunks_ = ctx.systemRegistry.get<ChunkPipelineSystem>();
    physics_ = ctx.systemRegistry.get<PhysicsGameSystem>();
    audio_ = ctx.systemRegistry.get<AudioGameSystem>();
    ai_ = ctx.systemRegistry.get<AIGameSystem>();
    terrain_ = ctx.systemRegistry.get<TerrainSystem>();
    charMovement_ = ctx.systemRegistry.get<CharacterMovementSystem>();
    lodSystem_ = ctx.systemRegistry.get<LODSystem>();
    meshSystem_ = ctx.systemRegistry.get<VoxelMeshingSystem>();
    voxelSim_ = ctx.systemRegistry.get<VoxelSimulationSystem>();

    debugDraw_.init();
    debugHUD_.init(ctx.rmlContext);
    chunkDebugPanel_.init(ctx.rmlContext);
    lodStatsPanel_.init(ctx.rmlContext);
    concurrencyPanel_.init(ctx.rmlContext);
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

    // Alt+C: Cycle camera projection mode (Panini -> Equirect -> Panini)
    auto* sceneView = ctx.sceneView;
    inputRouter->registerKeyCallback(SDLK_C, SDL_KMOD_ALT, [sceneView]() { sceneView->cycleProjectionMode(); });

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
    dispatcher.addEventListener("toggle_lod_stats", [this](fabric::Event&) { lodStatsPanel_.toggle(); });
    dispatcher.addEventListener("toggle_concurrency", [this](fabric::Event&) { concurrencyPanel_.toggle(); });

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
            if (physics_->physicsWorld().chunkCollisionShapeCount(coord.x, coord.y, coord.z) > 0) {
                // Convert to camera-relative coordinates
                auto worldOrigin =
                    fabric::Vector3<double, fabric::Space::World>(static_cast<double>(coord.x * recurse::K_CHUNK_SIZE),
                                                                  static_cast<double>(coord.y * recurse::K_CHUNK_SIZE),
                                                                  static_cast<double>(coord.z * recurse::K_CHUNK_SIZE));
                auto relOrigin = ctx.camera->cameraRelative(worldOrigin);

                float x0 = relOrigin.x;
                float y0 = relOrigin.y;
                float z0 = relOrigin.z;
                float x1 = x0 + static_cast<float>(recurse::K_CHUNK_SIZE);
                float y1 = y0 + static_cast<float>(recurse::K_CHUNK_SIZE);
                float z1 = z0 + static_cast<float>(recurse::K_CHUNK_SIZE);
                debugDraw_.drawWireBox(x0, y0, z0, x1, y1, z1);
            }
        }
    }

    // BVH overlay with depth coloring (F6)
    if (debugDraw_.hasFlag(recurse::DebugDrawFlags::BVHOverlay)) {
        fabric::BVH<int> chunkBVH;
        chunkBVH.beginBatch();
        int idx = 0;
        for (const auto& [coord, ent] : chunks_->chunkEntities()) {
            // Use world coordinates for BVH construction
            float x0 = static_cast<float>(coord.x * recurse::K_CHUNK_SIZE);
            float y0 = static_cast<float>(coord.y * recurse::K_CHUNK_SIZE);
            float z0 = static_cast<float>(coord.z * recurse::K_CHUNK_SIZE);
            float x1 = x0 + static_cast<float>(recurse::K_CHUNK_SIZE);
            float y1 = y0 + static_cast<float>(recurse::K_CHUNK_SIZE);
            float z1 = z0 + static_cast<float>(recurse::K_CHUNK_SIZE);
            chunkBVH.insert(fabric::AABB(fabric::Vec3f(x0, y0, z0), fabric::Vec3f(x1, y1, z1)), idx++);
        }
        chunkBVH.commitBatch();

        // Get camera world position for coordinate conversion
        auto camWorldPos = ctx.camera->worldPositionD();

        constexpr float K_MAX_VIS_DEPTH = 8.0f;
        chunkBVH.visitNodes([&](const fabric::AABB& bounds, int depth, bool isLeaf) {
            uint32_t color;
            if (isLeaf) {
                color = 0xff00ffff; // yellow for leaves (ABGR)
            } else {
                float t = std::min(1.0f, static_cast<float>(depth) / K_MAX_VIS_DEPTH);
                auto r = static_cast<uint8_t>((1.0f - t) * 255);
                auto b = static_cast<uint8_t>(t * 255);
                color = 0xff000000u | (static_cast<uint32_t>(b) << 16) | static_cast<uint32_t>(r);
            }
            // Convert to camera-relative coordinates for rendering
            float relMinX = static_cast<float>(bounds.min.x - camWorldPos.x);
            float relMinY = static_cast<float>(bounds.min.y - camWorldPos.y);
            float relMinZ = static_cast<float>(bounds.min.z - camWorldPos.z);
            float relMaxX = static_cast<float>(bounds.max.x - camWorldPos.x);
            float relMaxY = static_cast<float>(bounds.max.y - camWorldPos.y);
            float relMaxZ = static_cast<float>(bounds.max.z - camWorldPos.z);
            debugDraw_.setColor(color);
            debugDraw_.drawWireBox(relMinX, relMinY, relMinZ, relMaxX, relMaxY, relMaxZ);
        });
    }

    // Chunk state visualization (F1)
    if (debugDraw_.hasFlag(recurse::DebugDrawFlags::ChunkStates) && voxelSim_) {
        auto& tracker = voxelSim_->activityTracker();
        auto& grid = voxelSim_->simulationGrid();

        constexpr int K_CHUNK_DEBUG_VIS_RADIUS = 6;
        constexpr int K_VIS_RADIUS_SQ = K_CHUNK_DEBUG_VIS_RADIUS * K_CHUNK_DEBUG_VIS_RADIUS;
        constexpr float K_MIN_ALPHA_FRACTION = 0.2f;

        auto camWorldPos = ctx.camera->worldPositionD();
        int camChunkX = static_cast<int>(std::floor(camWorldPos.x / recurse::K_CHUNK_SIZE));
        int camChunkY = static_cast<int>(std::floor(camWorldPos.y / recurse::K_CHUNK_SIZE));
        int camChunkZ = static_cast<int>(std::floor(camWorldPos.z / recurse::K_CHUNK_SIZE));

        for (auto [cx, cy, cz] : grid.allChunks()) {
            recurse::simulation::ChunkCoord pos{cx, cy, cz};
            auto state = tracker.getState(pos);

            if (state == recurse::simulation::ChunkState::Sleeping)
                continue;

            int dx = cx - camChunkX;
            int dy = cy - camChunkY;
            int dz = cz - camChunkZ;
            int distSq = dx * dx + dy * dy + dz * dz;
            if (distSq > K_VIS_RADIUS_SQ)
                continue;

            float t = static_cast<float>(distSq) / static_cast<float>(K_VIS_RADIUS_SQ);
            float alphaFrac = 1.0f - t * (1.0f - K_MIN_ALPHA_FRACTION);

            uint32_t baseColor;
            switch (state) {
                case recurse::simulation::ChunkState::Active:
                    baseColor = 0xcc4de666; // Bright green
                    break;
                case recurse::simulation::ChunkState::BoundaryDirty:
                    baseColor = 0xb3cc33ff; // Yellow-orange
                    break;
                default:
                    baseColor = 0x80808080; // Gray
                    break;
            }

            auto baseAlpha = static_cast<uint8_t>(baseColor >> 24);
            auto newAlpha = static_cast<uint8_t>(static_cast<float>(baseAlpha) * alphaFrac);
            uint32_t color = (baseColor & 0x00FFFFFFu) | (static_cast<uint32_t>(newAlpha) << 24);

            auto worldOrigin = fabric::Vector3<double, fabric::Space::World>(
                static_cast<double>(cx * recurse::K_CHUNK_SIZE), static_cast<double>(cy * recurse::K_CHUNK_SIZE),
                static_cast<double>(cz * recurse::K_CHUNK_SIZE));
            auto relOrigin = ctx.camera->cameraRelative(worldOrigin);

            float x0 = relOrigin.x;
            float y0 = relOrigin.y;
            float z0 = relOrigin.z;
            float x1 = x0 + static_cast<float>(recurse::K_CHUNK_SIZE);
            float y1 = y0 + static_cast<float>(recurse::K_CHUNK_SIZE);
            float z1 = z0 + static_cast<float>(recurse::K_CHUNK_SIZE);

            debugDraw_.setColor(color);
            debugDraw_.drawWireBox(x0, y0, z0, x1, y1, z1);
        }

        // ChunkSlotState overlay (pipeline lifecycle; larger inset wireframe)
        auto& registry = grid.registry();
        for (auto [cx, cy, cz] : grid.allChunks()) {
            auto* slot = registry.find(cx, cy, cz);
            if (!slot)
                continue;

            if (slot->state == recurse::simulation::ChunkSlotState::Absent ||
                slot->state == recurse::simulation::ChunkSlotState::Active)
                continue;

            int dx = cx - camChunkX;
            int dy = cy - camChunkY;
            int dz = cz - camChunkZ;
            int distSq = dx * dx + dy * dy + dz * dz;
            if (distSq > K_VIS_RADIUS_SQ)
                continue;

            float t = static_cast<float>(distSq) / static_cast<float>(K_VIS_RADIUS_SQ);
            float alphaFrac = 1.0f - t * (1.0f - K_MIN_ALPHA_FRACTION);

            uint32_t baseSlotColor;
            switch (slot->state) {
                case recurse::simulation::ChunkSlotState::Generating:
                    baseSlotColor = 0xcc00ffff; // Yellow (ABGR)
                    break;
                case recurse::simulation::ChunkSlotState::Draining:
                    baseSlotColor = 0xcc0000ff; // Red (ABGR)
                    break;
                default:
                    continue;
            }

            auto baseAlpha = static_cast<uint8_t>(baseSlotColor >> 24);
            auto newAlpha = static_cast<uint8_t>(static_cast<float>(baseAlpha) * alphaFrac);
            uint32_t slotColor = (baseSlotColor & 0x00FFFFFFu) | (static_cast<uint32_t>(newAlpha) << 24);

            auto worldOrigin = fabric::Vector3<double, fabric::Space::World>(
                static_cast<double>(cx * recurse::K_CHUNK_SIZE), static_cast<double>(cy * recurse::K_CHUNK_SIZE),
                static_cast<double>(cz * recurse::K_CHUNK_SIZE));
            auto relOrigin = ctx.camera->cameraRelative(worldOrigin);

            constexpr float K_INSET = 2.0f;
            float x0 = relOrigin.x + K_INSET;
            float y0 = relOrigin.y + K_INSET;
            float z0 = relOrigin.z + K_INSET;
            float x1 = x0 + static_cast<float>(recurse::K_CHUNK_SIZE) - 2.0f * K_INSET;
            float y1 = y0 + static_cast<float>(recurse::K_CHUNK_SIZE) - 2.0f * K_INSET;
            float z1 = z0 + static_cast<float>(recurse::K_CHUNK_SIZE) - 2.0f * K_INSET;

            debugDraw_.setColor(slotColor);
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
        debugData.totalChunks = static_cast<int>(voxelSim_->simulationGrid().chunkCount());
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
        auto hit = recurse::castRay(voxelSim_->simulationGrid(), camPos.x, camPos.y, camPos.z, camFwd.x, camFwd.y,
                                    camFwd.z, 20.0f);
        if (hit.has_value()) {
            waila.hasHit = true;
            waila.voxelX = hit->x;
            waila.voxelY = hit->y;
            waila.voxelZ = hit->z;
            waila.chunkX = hit->x >> fabric::K_CHUNK_SHIFT;
            waila.chunkY = hit->y >> fabric::K_CHUNK_SHIFT;
            waila.chunkZ = hit->z >> fabric::K_CHUNK_SHIFT;
            waila.normalX = hit->nx;
            waila.normalY = hit->ny;
            waila.normalZ = hit->nz;
            waila.distance = hit->t;
            auto cell = voxelSim_->simulationGrid().readCell(hit->x, hit->y, hit->z);
            waila.density = (cell.materialId != recurse::simulation::material_ids::AIR) ? 1.0f : 0.0f;
            const auto* pal = voxelSim_->simulationGrid().chunkPalette(waila.chunkX, waila.chunkY, waila.chunkZ);
            if (pal && cell.essenceIdx < pal->paletteSize()) {
                auto e = pal->lookup(cell.essenceIdx);
                waila.essenceO = e.x;
                waila.essenceC = e.y;
                waila.essenceL = e.z;
                waila.essenceD = e.w;
            }
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
        constexpr float K_VERTEX_SIZE = sizeof(recurse::SmoothVoxelVertex);
        constexpr float K_INDEX_SIZE = sizeof(uint32_t);
        constexpr float K_MB = 1024.0f * 1024.0f;
        chunkData.vertexBufferMB = static_cast<float>(chunkData.vertexCount) * K_VERTEX_SIZE / K_MB;
        chunkData.indexBufferMB = static_cast<float>(chunkData.indexCount) * K_INDEX_SIZE / K_MB;

        auto meshingInfo = meshSystem_->debugInfo();
        chunkData.meshedThisFrame = meshingInfo.chunksMeshedThisFrame;
        chunkData.emptyChunksSkipped = meshingInfo.emptyChunksSkipped;
        chunkData.meshBudgetRemaining = meshingInfo.budgetRemaining;

        chunkDebugPanel_.update(chunkData);
    }

    // LOD stats panel
    if (lodStatsPanel_.isVisible() && lodSystem_) {
        auto lodInfo = lodSystem_->debugInfo();
        fabric::LODStatsData lodData;
        lodData.pendingSections = lodInfo.pendingSections;
        lodData.gpuResidentSections = lodInfo.gpuResidentSections;
        lodData.visibleSections = lodInfo.visibleSections;
        lodData.estimatedGpuMB = static_cast<float>(lodInfo.estimatedGpuBytes) / (1024.0f * 1024.0f);
        lodStatsPanel_.update(lodData);
    }

    // Concurrency panel
    if (concurrencyPanel_.isVisible() && voxelSim_) {
        auto concInfo = voxelSim_->scheduler().debugInfo();
        fabric::ConcurrencyData concData;
        concData.activeWorkers = concInfo.activeWorkers;
        concData.queuedJobs = concInfo.queuedJobs;
        concurrencyPanel_.update(concData);
    }
}

void DebugOverlaySystem::doShutdown() {
    devConsole_.shutdown();
    contentBrowser_.shutdown();
    btDebugPanel_.shutdown();
    concurrencyPanel_.shutdown();
    lodStatsPanel_.shutdown();
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
    after<LODSystem>();
    after<PhysicsGameSystem>();
    after<AudioGameSystem>();
    after<AIGameSystem>();
    after<TerrainSystem>();
    after<CharacterMovementSystem>();
    after<VoxelSimulationSystem>();
    after<VoxelMeshingSystem>();
}

} // namespace recurse::systems
