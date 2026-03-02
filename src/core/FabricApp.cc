#include "fabric/core/FabricApp.hh"
#include "fabric/core/AppContext.hh"
#include "fabric/core/AssetRegistry.hh"
#include "fabric/core/ConfigManager.hh"
#include "fabric/core/Log.hh"
#include "fabric/core/ResourceHub.hh"
#include "fabric/core/RuntimeState.hh"
#include "fabric/core/SystemRegistry.hh"
#include "fabric/utils/Profiler.hh"

#include <cstring>

namespace fabric {

int FabricApp::run(int argc, char** argv, FabricAppDesc desc) {
    // ── Phase 1: Bootstrap ──────────────────────────────────────
    // Logging is initialized by the caller (main() or test harness).
    FABRIC_LOG_INFO("Starting {}", desc.name);

    // Parse --help / --version early exit
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--help") == 0 || std::strcmp(argv[i], "-h") == 0) {
            FABRIC_LOG_INFO("Usage: {} [options]", desc.name);
            return 0;
        }
        if (std::strcmp(argv[i], "--version") == 0 || std::strcmp(argv[i], "-v") == 0) {
            FABRIC_LOG_INFO("{} v0.1.0", desc.name);
            return 0;
        }
    }

    // ── Phase 2: Platform Init ──────────────────────────────────
    // TODO: SDL_Init, PlatformInfo::populate(), createWindow, bgfx::init
    // These require runtime SDL/bgfx and are wired in a future integration pass.

    // ── Phase 3: Infrastructure ─────────────────────────────────
    ConfigManager configManager;
    if (!desc.configPath.empty())
        configManager.loadAppConfig(desc.configPath);
    configManager.applyCLIOverrides(argc, argv);

    EventDispatcher dispatcher;
    Timeline timeline;
    World world;
    ResourceHub resourceHub;
    AssetRegistry assetRegistry(resourceHub);
    SystemRegistry systemRegistry;
    RuntimeState runtimeState;

    AppContext ctx{
        .world = world,
        .timeline = timeline,
        .dispatcher = dispatcher,
        .resourceHub = resourceHub,
        .assetRegistry = assetRegistry,
        .systemRegistry = systemRegistry,
        .configManager = configManager,
        .runtimeState = &runtimeState,
    };

    // ── Phase 4: System Registration ────────────────────────────
    for (auto& reg : desc.systemRegistrations_) {
        auto system = reg.factory();
        systemRegistry.registerSystem(reg.phase, std::move(system));
    }

    // ── Phase 5: Dependency Resolution ──────────────────────────
    if (!systemRegistry.resolve()) {
        FABRIC_LOG_CRITICAL("System dependency cycle detected");
        return 1;
    }

    // ── Phase 6: System Init ────────────────────────────────────
    try {
        systemRegistry.initAll(ctx);
    } catch (const std::exception& e) {
        FABRIC_LOG_CRITICAL("System initialization failed: {}", e.what());
        return 1;
    }

    // ── Phase 7: Application Init ───────────────────────────────
    if (desc.onInit)
        desc.onInit(ctx);

    FABRIC_LOG_INFO("All systems initialized");

    // ── Phase 8: Main Loop ──────────────────────────────────────
    // TODO: Full SDL event loop with fixed timestep accumulator.
    // The main loop requires SDL event polling and bgfx::frame().
    // For now, this is a skeleton; the loop is wired during platform
    // integration.

    // ── Phase 9: Shutdown ───────────────────────────────────────
    FABRIC_LOG_INFO("Shutting down");

    if (desc.onShutdown)
        desc.onShutdown(ctx);

    systemRegistry.shutdownAll();
    return 0;
}

} // namespace fabric
