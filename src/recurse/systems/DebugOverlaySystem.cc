#include "recurse/systems/DebugOverlaySystem.hh"

#include "recurse/input/ActionIds.hh"

#include "fabric/core/AppContext.hh"
#include "fabric/core/AppModeManager.hh"
#include "fabric/core/Event.hh"
#include "fabric/core/SystemRegistry.hh"
#include "fabric/core/WorldLifecycle.hh"
#include "fabric/input/InputRouter.hh"
#include "fabric/log/Log.hh"
#include "fabric/platform/JobScheduler.hh"
#include "fabric/render/Camera.hh"
#include "fabric/render/SceneView.hh"
#include "fabric/utils/BVH.hh"
#include "fabric/utils/Profiler.hh"
#include "recurse/ai/BehaviorAI.hh"
#include "recurse/character/MovementFSM.hh"
#include "recurse/persistence/WorldSession.hh"
#include "recurse/physics/PhysicsWorld.hh"
#include "recurse/simulation/ChunkActivityTracker.hh"
#include "recurse/simulation/ChunkRegistry.hh"
#include "recurse/simulation/ChunkState.hh"
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
#include <Jolt/Physics/PhysicsSystem.h>

#include <bgfx/bgfx.h>
#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace recurse::systems {

namespace {

std::string formatAutosaveSeconds(float seconds) {
    char buffer[16];
    std::snprintf(buffer, sizeof(buffer), "%.1fs", std::max(0.0f, seconds));
    return buffer;
}

} // namespace

void DebugOverlaySystem::doInit(fabric::AppContext& ctx) {
    if (auto* wl = ctx.worldLifecycle) {
        wl->registerParticipant([this]() { onWorldBegin(); }, [this]() { onWorldEnd(); });
    }
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
    voxelRender_ = ctx.systemRegistry.get<VoxelRenderSystem>();
    voxelSim_ = ctx.systemRegistry.get<VoxelSimulationSystem>();

    debugDraw_.init();
    debugHUD_.init(ctx.rmlContext);
    autosavePanel_.init(ctx.rmlContext);
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
    using namespace recurse::input;

    dispatcher.addEventListener(K_ACTION_TOGGLE_DEBUG, [this](fabric::Event&) {
        debugHUD_.toggle();
        // WAILA and HotkeyPanel are always visible now
    });

    dispatcher.addEventListener(K_ACTION_TOGGLE_CHUNK_DEBUG, [this](fabric::Event&) {
        chunkDebugPanel_.toggle();
        FABRIC_LOG_INFO("Chunk Debug (F12): {}", chunkDebugPanel_.isVisible() ? "on" : "off");
    });
    dispatcher.addEventListener(K_ACTION_TOGGLE_LOD_STATS, [this](fabric::Event&) { lodStatsPanel_.toggle(); });
    dispatcher.addEventListener(K_ACTION_TOGGLE_CONCURRENCY, [this](fabric::Event&) { concurrencyPanel_.toggle(); });

    dispatcher.addEventListener(K_ACTION_TOGGLE_WIREFRAME, [this](fabric::Event&) {
        debugDraw_.toggleWireframe();
        const bool enabled = debugDraw_.isWireframeEnabled();
        if (voxelRender_) {
            voxelRender_->voxelRenderer().setWireframeEnabled(enabled);
        }
        FABRIC_LOG_INFO("Voxel Wireframe (F4): {}", enabled ? "on" : "off");
    });

    dispatcher.addEventListener(K_ACTION_TOGGLE_COLLISION_DEBUG, [this](fabric::Event&) {
        debugDraw_.toggleFlag(recurse::DebugDrawFlags::CollisionShapes);
        FABRIC_LOG_INFO("Collision shapes: {}",
                        debugDraw_.hasFlag(recurse::DebugDrawFlags::CollisionShapes) ? "on" : "off");
    });

    dispatcher.addEventListener(K_ACTION_TOGGLE_BVH_DEBUG, [this](fabric::Event&) {
        debugDraw_.toggleFlag(recurse::DebugDrawFlags::BVHOverlay);
        FABRIC_LOG_INFO("BVH overlay: {}", debugDraw_.hasFlag(recurse::DebugDrawFlags::BVHOverlay) ? "on" : "off");
    });

    dispatcher.addEventListener(K_ACTION_TOGGLE_CHUNK_STATES, [this](fabric::Event&) {
        debugDraw_.toggleFlag(recurse::DebugDrawFlags::ChunkStates);
        FABRIC_LOG_INFO("Chunk states: {}", debugDraw_.hasFlag(recurse::DebugDrawFlags::ChunkStates) ? "on" : "off");
    });

    dispatcher.addEventListener(K_ACTION_TOGGLE_CONTENT_BROWSER, [this, appMode](fabric::Event&) {
        auto mode = appMode->current();
        if (mode == fabric::AppMode::Editor) {
            appMode->transition(fabric::AppMode::Game);
        } else if (mode == fabric::AppMode::Game) {
            appMode->transition(fabric::AppMode::Editor);
        }
        contentBrowser_.toggle();
        FABRIC_LOG_INFO("Content Browser: {}", contentBrowser_.isVisible() ? "on" : "off");
    });

    dispatcher.addEventListener(K_ACTION_TOGGLE_BT_DEBUG, [this](fabric::Event&) {
        btDebugPanel_.toggle();
        FABRIC_LOG_INFO("BT Debug (F11): {}", btDebugPanel_.isVisible() ? "on" : "off");
    });

    dispatcher.addEventListener(K_ACTION_CYCLE_BT_NPC, [this, &ctx](fabric::Event&) {
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

    auto* sceneView = ctx.sceneView;

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

        for (auto [cx, cy, cz] : grid.allChunks()) {
            recurse::simulation::ChunkCoord pos{cx, cy, cz};
            auto state = tracker.getState(pos);

            if (state == recurse::simulation::ChunkState::Sleeping)
                continue;

            uint32_t color;
            switch (state) {
                case recurse::simulation::ChunkState::Active:
                    color = 0xcc4de666; // Bright green
                    break;
                case recurse::simulation::ChunkState::BoundaryDirty:
                    color = 0xb3cc33ff; // Yellow-orange
                    break;
                default:
                    color = 0x80808080; // Gray
                    break;
            }

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
            uint32_t slotColor;
            if (recurse::simulation::findAs<recurse::simulation::Generating>(registry, cx, cy, cz))
                slotColor = 0xcc00ffff; // Yellow (ABGR)
            else if (recurse::simulation::findAs<recurse::simulation::Draining>(registry, cx, cy, cz))
                slotColor = 0xcc0000ff; // Red (ABGR)
            else
                continue;

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
        DebugData debugData;
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

        recurse::AutosaveIndicatorData autosaveData;
        if (chunks_ && chunks_->session()) {
            auto saveStatus = chunks_->session()->runtimeStatusSnapshot().saveActivity;
            size_t pendingDirty = saveStatus.dirtyChunks >= saveStatus.savingChunks
                                      ? (saveStatus.dirtyChunks - saveStatus.savingChunks)
                                      : 0;
            bool hasSaving = saveStatus.savingChunks > 0;
            bool hasPending = pendingDirty > 0 || saveStatus.preparedChunks > 0;

            debugData.autosaveDirtyChunks = static_cast<int>(pendingDirty);
            debugData.autosaveSavingChunks = static_cast<int>(saveStatus.savingChunks);
            debugData.autosaveQueuedChunks = static_cast<int>(saveStatus.preparedChunks);
            if (saveStatus.secondsUntilNextSave >= 0.0f) {
                debugData.autosaveNextSave = formatAutosaveSeconds(saveStatus.secondsUntilNextSave);
            } else if (saveStatus.preparedChunks > 0) {
                debugData.autosaveNextSave = "queued";
            }

            // ChunkSaveService's debounce and max-delay batches are both
            // change-driven persistence. They stay active for data safety, but
            // the user-facing autosave popup is reserved for failure reporting
            // until there is a distinct wall-clock autosave path.
            if (saveStatus.hasError) {
                autosaveData.visible = true;
                autosaveData.statusText = "Autosave issue";
                autosaveData.detailText =
                    saveStatus.lastError.empty() ? "Save failed. Retry pending." : saveStatus.lastError;
                debugData.autosaveState = "Issue";
            } else if (hasSaving) {
                debugData.autosaveState = "Saving";
            } else if (hasPending) {
                debugData.autosaveState = "Pending";
            } else {
                debugData.autosaveState = "Idle";
            }
        }
        debugHUD_.update(debugData);
        autosavePanel_.update(autosaveData);

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
        WAILAData waila;
        auto camPos = camera_->position();
        auto camFwd = camera_->forward();
        auto hit = recurse::castRay(voxelSim_->simulationGrid(), camPos.x, camPos.y, camPos.z, camFwd.x, camFwd.y,
                                    camFwd.z, 20.0f);
        if (hit.has_value()) {
            waila.hasHit = true;
            waila.voxelX = hit->x;
            waila.voxelY = hit->y;
            waila.voxelZ = hit->z;
            waila.chunkX = hit->x >> recurse::simulation::K_CHUNK_SHIFT;
            waila.chunkY = hit->y >> recurse::simulation::K_CHUNK_SHIFT;
            waila.chunkZ = hit->z >> recurse::simulation::K_CHUNK_SHIFT;
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
        ChunkDebugData chunkData;
        chunkData.trackedChunks = voxelSim_ ? voxelSim_->simulationGrid().chunkCount() : 0;
        chunkData.activeChunks = voxelSim_ ? voxelSim_->activeChunkCount() : 0;
        chunkData.chunksRendered = voxelRender_ ? voxelRender_->renderedChunkCount() : 0;
        chunkData.gpuMeshCount = meshSystem_->gpuMeshCount();
        chunkData.meshCandidateChunks = meshSystem_->pendingMeshCount();
        chunkData.vertexCount = meshSystem_->vertexBufferSize();
        chunkData.indexCount = meshSystem_->indexBufferSize();

        if (lodSystem_) {
            auto lodInfo = lodSystem_->debugInfo();
            chunkData.lodVisibleSections = static_cast<size_t>(lodInfo.visibleSections);
            chunkData.lodGpuSectionCount = static_cast<size_t>(lodInfo.gpuResidentSections);
            chunkData.lodPendingSections = static_cast<size_t>(lodInfo.pendingSections);
        }

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
        LODStatsData lodData;
        lodData.pendingSections = lodInfo.pendingSections;
        lodData.gpuResidentSections = lodInfo.gpuResidentSections;
        lodData.visibleSections = lodInfo.visibleSections;
        lodData.fullResRejectedSections = lodInfo.fullResRejectedSections;
        lodData.fullResCenterCX = lodInfo.fullResCenterCX;
        lodData.fullResCenterCY = lodInfo.fullResCenterCY;
        lodData.fullResCenterCZ = lodInfo.fullResCenterCZ;
        lodData.fullResRadius = lodInfo.fullResRadius;
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
    autosavePanel_.shutdown();
    hotkeyPanel_.shutdown();
    wailaPanel_.shutdown();
    debugHUD_.shutdown();
    debugDraw_.shutdown();
    FABRIC_LOG_INFO("DebugOverlaySystem shut down");
}

void DebugOverlaySystem::onWorldBegin() {}

void DebugOverlaySystem::onWorldEnd() {
    btDebugSelectedNpc_ = flecs::entity{};
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
