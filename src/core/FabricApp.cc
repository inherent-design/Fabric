#include "fabric/core/FabricApp.hh"
#include "fabric/core/AppContext.hh"
#include "fabric/core/AppModeManager.hh"
#include "fabric/core/AssetRegistry.hh"
#include "fabric/core/Async.hh"
#include "fabric/core/BgfxCallback.hh"
#include "fabric/core/Camera.hh"
#include "fabric/core/ConfigManager.hh"
#include "fabric/core/InputManager.hh"
#include "fabric/core/InputRouter.hh"
#include "fabric/core/InputSystem.hh"
#include "fabric/core/Log.hh"
#include "fabric/core/RenderCaps.hh"
#include "fabric/core/ResourceHub.hh"
#include "fabric/core/RuntimeState.hh"
#include "fabric/core/SceneView.hh"
#include "fabric/core/SystemRegistry.hh"
#include "fabric/platform/CursorManager.hh"
#include "fabric/platform/PlatformInfo.hh"
#include "fabric/platform/WindowDesc.hh"
#include "fabric/ui/BgfxRenderInterface.hh"
#include "fabric/ui/BgfxSystemInterface.hh"
#include "fabric/utils/Profiler.hh"

#include <RmlUi/Core.h>

#include <bgfx/bgfx.h>
#include <bgfx/platform.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_properties.h>

#include <chrono>
#include <cstring>

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

} // namespace

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
    SDL_Window* window = nullptr;
    Rml::Context* rmlContext = nullptr;
    BgfxSystemInterface rmlSystem;
    BgfxRenderInterface rmlRenderer;

    if (!desc.headless) {
        if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
            FABRIC_LOG_CRITICAL("SDL init failed: {}", SDL_GetError());
            return 1;
        }

        // Use WindowDesc from the descriptor (config overrides can adjust it)
        window = createWindow(desc.windowDesc);
        if (!window) {
            FABRIC_LOG_CRITICAL("Window creation failed: {}", SDL_GetError());
            SDL_Quit();
            return 1;
        }

        // Single-threaded rendering: call renderFrame() before bgfx::init().
        // On macOS Metal must stay on the main thread.
        bgfx::renderFrame();

        int pw, ph;
        SDL_GetWindowSizeInPixels(window, &pw, &ph);

        bgfx::Init bgfxInit;
        bgfxInit.type = bgfx::RendererType::Vulkan;
        bgfxInit.platformData = getPlatformData(window);
        bgfxInit.resolution.width = static_cast<uint32_t>(pw);
        bgfxInit.resolution.height = static_cast<uint32_t>(ph);
        bgfxInit.resolution.reset = BGFX_RESET_VSYNC | BGFX_RESET_HIDPI;

        // Route bgfx internal diagnostics through Quill (respects console_exclude patterns)
        static BgfxCallback bgfxCallback;
        bgfxInit.callback = &bgfxCallback;

        if (!bgfx::init(bgfxInit)) {
            FABRIC_LOG_CRITICAL("bgfx init failed");
            SDL_DestroyWindow(window);
            SDL_Quit();
            return 1;
        }

        bgfx::setViewClear(0, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x303030ff, 1.0f, 0);
        bgfx::setViewRect(0, 0, 0, static_cast<uint16_t>(pw), static_cast<uint16_t>(ph));

        FABRIC_LOG_INFO("bgfx renderer: {}", bgfx::getRendererName(bgfx::getRendererType()));

        // RmlUi backend interfaces
        rmlRenderer.init();

        Rml::SetSystemInterface(&rmlSystem);
        Rml::SetRenderInterface(&rmlRenderer);
        Rml::Initialise();

        rmlContext = Rml::CreateContext("main", Rml::Vector2i(pw, ph));
        FABRIC_LOG_INFO("RmlUi context created ({}x{})", pw, ph);

        // Set DPI scale ratio for HiDPI displays
        int winW, winH;
        SDL_GetWindowSize(window, &winW, &winH);
        if (winW > 0 && winH > 0) {
            float dpiScale = static_cast<float>(pw) / static_cast<float>(winW);
            rmlContext->SetDensityIndependentPixelRatio(dpiScale);
            FABRIC_LOG_INFO("RmlUi DPI scale: {:.2f} (window {}x{}, pixels {}x{})", dpiScale, winW, winH, pw, ph);
        }

        if (!Rml::LoadFontFace("assets/fonts/robotomono-regular.ttf", true)) {
            FABRIC_LOG_WARN("Failed to load RmlUi font: assets/fonts/robotomono-regular.ttf");
        }

        async::init();
    }

    // ── Phase 3: Infrastructure ─────────────────────────────────
    ConfigManager configManager;
    configManager.loadEngineConfig(ConfigManager::findConfig("fabric.toml"));
    if (!desc.configPath.empty())
        configManager.loadAppConfig(ConfigManager::findConfig(desc.configPath));
    configManager.applyCLIOverrides(argc, argv);

    EventDispatcher dispatcher;
    Timeline timeline;
    World world;
    ResourceHub resourceHub;
    AssetRegistry assetRegistry(resourceHub);
    SystemRegistry systemRegistry;
    RuntimeState runtimeState;

    // Engine objects (constructed only in non-headless mode)
    std::unique_ptr<InputManager> inputManagerPtr;
    std::unique_ptr<InputRouter> inputRouterPtr;
    std::unique_ptr<InputSystem> inputSystemPtr;
    std::unique_ptr<Camera> camera;
    std::unique_ptr<AppModeManager> appModeManager;
    std::unique_ptr<SceneView> sceneView;
    std::unique_ptr<PlatformInfo> platformInfoPtr;
    std::unique_ptr<RenderCaps> renderCapsPtr;
    std::unique_ptr<CursorManager> cursorManagerPtr;

    if (!desc.headless) {
        inputManagerPtr = std::make_unique<InputManager>(dispatcher);
        inputRouterPtr = std::make_unique<InputRouter>(*inputManagerPtr);
        inputRouterPtr->setMode(InputMode::GameOnly);
        inputRouterPtr->setWindow(window);
        inputSystemPtr = std::make_unique<InputSystem>(dispatcher);

        int pw, ph;
        SDL_GetWindowSizeInPixels(window, &pw, &ph);
        float aspect = static_cast<float>(pw) / static_cast<float>(ph);
        bool homogeneousNdc = bgfx::getCaps()->homogeneousDepth;

        camera = std::make_unique<Camera>();
        camera->setPerspective(60.0f, aspect, 0.1f, 1000.0f, homogeneousNdc);

        appModeManager = std::make_unique<AppModeManager>();
        sceneView = std::make_unique<SceneView>(0, *camera, world.get());
        sceneView->setViewport(static_cast<uint16_t>(pw), static_cast<uint16_t>(ph));

        platformInfoPtr = std::make_unique<PlatformInfo>();
        platformInfoPtr->populate();
        platformInfoPtr->populateGPU();

        renderCapsPtr = std::make_unique<RenderCaps>();
        renderCapsPtr->initFromBgfx();

        cursorManagerPtr = std::make_unique<CursorManager>(window);
    }

    AppContext ctx{
        .world = world,
        .timeline = timeline,
        .dispatcher = dispatcher,
        .resourceHub = resourceHub,
        .assetRegistry = assetRegistry,
        .systemRegistry = systemRegistry,
        .configManager = configManager,
        .inputSystem = inputSystemPtr.get(),
        .runtimeState = &runtimeState,
        .platformInfo = platformInfoPtr.get(),
        .renderCaps = renderCapsPtr.get(),
        .appModeManager = appModeManager.get(),
        .window = window,
        .cursorManager = cursorManagerPtr.get(),
        .inputManager = inputManagerPtr.get(),
        .inputRouter = inputRouterPtr.get(),
        .camera = camera.get(),
        .sceneView = sceneView.get(),
        .rmlContext = rmlContext,
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

        // Clean up platform resources initialized in Phase 2
        if (!desc.headless) {
            Rml::Shutdown();
            rmlRenderer.shutdown();
            bgfx::shutdown();
            SDL_DestroyWindow(window);
            SDL_Quit();
            async::shutdown();
        }
        return 1;
    }

    // ── Phase 7: Application Init ───────────────────────────────
    if (desc.onInit)
        desc.onInit(ctx);

    FABRIC_LOG_INFO("All systems initialized");

    // ── Phase 8: Main Loop ──────────────────────────────────────
    if (!desc.headless) {
        constexpr double K_FIXED_DT = 1.0 / 60.0;
        double accumulator = 0.0;
        auto lastTime = std::chrono::high_resolution_clock::now();
        bool running = true;

        FABRIC_LOG_INFO("Entering main loop");

        while (running) {
            FABRIC_ZONE_SCOPED_N("main_loop");

            auto now = std::chrono::high_resolution_clock::now();
            double frameTime = std::chrono::duration<double>(now - lastTime).count();
            lastTime = now;

            // Cap frame time to prevent spiral after debugger pauses or hitches
            if (frameTime > 0.25)
                frameTime = 0.25;
            accumulator += frameTime;

            // Route SDL events
            {
                FABRIC_ZONE_SCOPED_N("input_routing");
                SDL_Event event;
                while (SDL_PollEvent(&event)) {
                    inputRouterPtr->routeEvent(event, rmlContext);

                    if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN && event.button.button == SDL_BUTTON_LEFT) {
                        if (appModeManager->current() == AppMode::Paused) {
                            // Only auto-resume if click didn't hit a UI element
                            // (i.e., hover element is body or null)
                            Rml::Element* hover = rmlContext->GetHoverElement();
                            bool isBody = !hover || hover->GetTagName() == "body";
                            if (isBody) {
                                appModeManager->transition(AppMode::Game);
                            }
                        }
                    }

                    if (event.type == SDL_EVENT_QUIT)
                        running = false;

                    if (event.type == SDL_EVENT_WINDOW_FOCUS_GAINED) {
                        if (desc.onFocusGained)
                            desc.onFocusGained(ctx);
                    }

                    if (event.type == SDL_EVENT_WINDOW_FOCUS_LOST) {
                        if (desc.onFocusLost)
                            desc.onFocusLost(ctx);
                    }

                    if (event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
                        auto w = static_cast<uint32_t>(event.window.data1);
                        auto h = static_cast<uint32_t>(event.window.data2);
                        if (w == 0 || h == 0)
                            continue;
                        bgfx::reset(w, h, BGFX_RESET_VSYNC | BGFX_RESET_HIDPI);
                        bgfx::setViewRect(0, 0, 0, static_cast<uint16_t>(w), static_cast<uint16_t>(h));
                        sceneView->setViewport(static_cast<uint16_t>(w), static_cast<uint16_t>(h));
                        float newAspect = static_cast<float>(w) / static_cast<float>(h);
                        bool homogNdc = bgfx::getCaps()->homogeneousDepth;
                        camera->setPerspective(60.0f, newAspect, 0.1f, 1000.0f, homogNdc);
                        rmlContext->SetDimensions(Rml::Vector2i(static_cast<int>(w), static_cast<int>(h)));

                        // Update DPI scale ratio on resize
                        int winW, winH;
                        SDL_GetWindowSize(window, &winW, &winH);
                        if (winW > 0 && winH > 0) {
                            float dpiScale = static_cast<float>(w) / static_cast<float>(winW);
                            rmlContext->SetDensityIndependentPixelRatio(dpiScale);
                        }

                        if (desc.onResize)
                            desc.onResize(ctx, w, h);
                    }
                }
            }

            // Pre-update: input processing, mode transitions (once per frame)
            systemRegistry.runPreUpdate(ctx, static_cast<float>(frameTime));

            // Fixed timestep simulation
            constexpr int K_MAX_FIXED_STEPS_PER_FRAME = 3;
            int fixedIter = 0;
            while (accumulator >= K_FIXED_DT && fixedIter < K_MAX_FIXED_STEPS_PER_FRAME) {
                ++fixedIter;
                FABRIC_ZONE_SCOPED_N("fixed_timestep");
                float dt = static_cast<float>(K_FIXED_DT);

                {
                    FABRIC_ZONE_SCOPED_N("async_poll");
                    async::poll();
                }
                {
                    FABRIC_ZONE_SCOPED_N("timeline_update");
                    timeline.update(K_FIXED_DT);
                }

                systemRegistry.runFixedUpdate(ctx, dt);

                accumulator -= K_FIXED_DT;
            }

            // Drain excess accumulator if we hit the iteration cap
            if (fixedIter >= K_MAX_FIXED_STEPS_PER_FRAME && accumulator > K_FIXED_DT) {
                FABRIC_ZONE_SCOPED_N("accumulator_drain");
                accumulator = 0.0;
            }

            // Per-frame logic: camera, audio, animation
            systemRegistry.runUpdate(ctx, static_cast<float>(frameTime));
            systemRegistry.runPostUpdate(ctx, static_cast<float>(frameTime));

            // Per-frame input reset
            inputRouterPtr->beginFrame();

            // Rendering phases
            systemRegistry.runPreRender(ctx);
            systemRegistry.runRender(ctx);
            systemRegistry.runPostRender(ctx);

            // RmlUi overlay
            {
                int curW, curH;
                SDL_GetWindowSizeInPixels(window, &curW, &curH);
                rmlRenderer.beginFrame(static_cast<uint16_t>(curW), static_cast<uint16_t>(curH));
                rmlContext->Update();
                rmlContext->Render();
            }

            // Persist user preferences (500ms debounce, no-op when clean)
            configManager.flushIfDirty();

            bgfx::frame();

            // RuntimeState per-frame update
            runtimeState.frameTimeMs = static_cast<float>(frameTime * 1000.0);
            runtimeState.fps = (frameTime > 0.0) ? static_cast<float>(1.0 / frameTime) : 0.0f;

            FABRIC_FRAME_MARK;
        }
    }

    // ── Phase 9: Shutdown ───────────────────────────────────────
    FABRIC_LOG_INFO("Shutting down");

    if (desc.onShutdown)
        desc.onShutdown(ctx);

    systemRegistry.shutdownAll();

    if (!desc.headless) {
        Rml::Shutdown();
        rmlRenderer.shutdown();
        bgfx::shutdown();
        SDL_DestroyWindow(window);
        SDL_Quit();
        async::shutdown();
    }

    return 0;
}

} // namespace fabric
