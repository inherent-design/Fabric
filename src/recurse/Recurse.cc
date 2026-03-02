// Recurse.cc -- Main entry point for the Recurse game.
// Constructs FabricAppDesc and delegates to FabricApp::run().
//
// Option A: Recurse is a secondary executable alongside Fabric.
// FabricApp::run() main loop (Phase 8) is still a skeleton.
// onInit/onShutdown are fully populated; onFixedUpdate/onRender
// are deferred to Phase 5 when the main loop is wired.
//
// Fabric.cc remains the primary working executable with its own
// main loop. This file demonstrates the FabricAppDesc pattern.

#include "fabric/core/AppContext.hh"
#include "fabric/core/AppModeManager.hh"
#include "fabric/core/Camera.hh"
#include "fabric/core/ConfigManager.hh"
#include "fabric/core/Constants.g.hh"
#include "fabric/core/ECS.hh"
#include "fabric/core/Event.hh"
#include "fabric/core/FabricApp.hh"
#include "fabric/core/FabricAppDesc.hh"
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
#include "recurse/world/FieldLayer.hh"
#include "recurse/world/TerrainGenerator.hh"
#include "recurse/world/VoxelMesher.hh"

#include "fabric/ui/DebugHUD.hh"
#include "fabric/ui/ToastManager.hh"
#include "fabric/utils/BVH.hh"
#include "fabric/utils/Profiler.hh"

#include <bgfx/bgfx.h>

#include <cmath>
#include <limits>
#include <unordered_map>
#include <unordered_set>

namespace {

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
                          recurse::DensityField& density, recurse::EssenceField& essence) {
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

} // namespace

/// Build the FabricAppDesc for the Recurse game.
///
/// FabricApp::run() owns: log, CLI parsing, config loading, infrastructure
/// objects (EventDispatcher, Timeline, World, ResourceHub, etc.), main loop,
/// and shutdown of engine systems.
///
/// Recurse provides via callbacks: application system construction, key
/// bindings, event listeners, game update logic, and render logic.
///
/// NOTE: FabricApp::run() Phase 8 (main loop) is still a skeleton. The
/// main loop calls onInit then immediately proceeds to shutdown. As a
/// result, onFixedUpdate and onRender are stubs. They will be wired in
/// Phase 5 when the main loop is fully implemented.
fabric::FabricAppDesc buildRecurseDesc() {
    fabric::FabricAppDesc desc;
    desc.name = "Recurse";
    desc.configPath = "recurse.toml";

    // -----------------------------------------------------------------
    // onInit: called after engine infrastructure is ready (Phase 7)
    // -----------------------------------------------------------------
    // Constructs all application systems, binds keys, registers event
    // listeners. Migrated from Fabric.cc init sections.
    //
    // NOTE: Systems that require SDL_Window, bgfx context, or RmlUi
    // context (DebugDraw, DebugHUD, BTDebugPanel, DevConsole, OIT,
    // SceneView viewport, Camera perspective) are NOT initialized here
    // because FabricApp::run() Phase 2 (platform init) is still a TODO.
    // These will be wired when platform init is implemented. For now,
    // only platform-independent application systems are constructed.
    desc.onInit = [](fabric::AppContext& ctx) {
        FABRIC_LOG_INFO("Recurse onInit: constructing application systems");

        auto& dispatcher = ctx.dispatcher;
        auto& timeline = ctx.timeline;
        auto& ecsWorld = ctx.world;

        // -- Key bindings ------------------------------------------------
        // TODO(phase-5): Wire key bindings through AppContext.inputSystem
        // once FabricApp::run() creates InputManager/InputRouter.
        // For now, key bindings are documented but not wired because
        // InputManager is not yet available via AppContext.
        //
        // Movement: W/S/A/D, Space, LShift
        // Time: P (pause), +/- (scale)
        // Toggles: F (fly), F3 (debug), F4 (wireframe), V (camera),
        //          F10 (collision debug), F6 (BVH), F7 (content browser),
        //          F11 (BT debug), F8 (cycle NPC)
        // Save: F5 (quicksave), F9 (quickload)

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

        // -- Terrain: density + essence fields, generator, cave carver ---
        // TODO(phase-5): These should be stored in a game context struct
        // accessible from onFixedUpdate/onRender. For now they are local
        // to onInit and will be restructured when callbacks share state.

        // -- Physics -----------------------------------------------------
        // TODO(phase-5): PhysicsWorld::init() requires knowing max bodies.
        // Deferred until game context struct is available.

        // -- Character systems -------------------------------------------
        // TODO(phase-5): CharacterController, FlightController, MovementFSM
        // need to persist across frames. Deferred to game context.

        // -- AI, Audio, Animation ----------------------------------------
        // TODO(phase-5): BehaviorAI, AudioSystem, Pathfinding, AnimationEvents
        // need ECS world and persistent state. Deferred to game context.

        // -- Save system -------------------------------------------------
        // TODO(phase-5): SaveManager, ToastManager need frame-persistent state.

        // -- Particle system + DebrisPool --------------------------------
        // TODO(phase-5): ParticleSystem, DebrisPool need persistent state.

        FABRIC_LOG_INFO("Recurse onInit complete (platform-independent systems)");
    };

    // -----------------------------------------------------------------
    // onShutdown: called before engine shutdown (Phase 9)
    // -----------------------------------------------------------------
    // Tears down application systems in reverse initialization order.
    // Once onInit constructs persistent systems, onShutdown will mirror.
    desc.onShutdown = [](fabric::AppContext& ctx) {
        FABRIC_LOG_INFO("Recurse onShutdown: tearing down application systems");

        // Shutdown order (reverse of init, mirrors Fabric.cc):
        // TODO(phase-5): Uncomment when systems are constructed in onInit.
        //
        // devConsole.shutdown();
        // contentBrowser.shutdown();
        // btDebugPanel.shutdown();
        // debugHUD.shutdown();
        // animEvents.shutdown();
        // pathfinding.shutdown();
        // behaviorAI.shutdown();
        // audioSystem.shutdown();
        // ragdoll.shutdown();
        // physicsWorld.shutdown();
        //
        // Destroy GPU meshes:
        // for (auto& [_, mesh] : gpuMeshes)
        //     recurse::VoxelMesher::destroyMesh(mesh);
        //
        // Destroy chunk ECS entities:
        // for (auto& [_, entity] : chunkEntities)
        //     entity.destruct();
        //
        // oitCompositor.shutdown();
        // particleSystem.shutdown();
        // voxelRenderer.shutdown();
        // debugDraw.shutdown();

        (void)ctx;
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
