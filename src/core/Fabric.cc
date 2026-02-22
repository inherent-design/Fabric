#include "fabric/core/Async.hh"
#include "fabric/core/Camera.hh"
#include "fabric/core/Constants.g.hh"
#include "fabric/core/Event.hh"
#include "fabric/core/InputManager.hh"
#include "fabric/core/Log.hh"
#include "fabric/core/SceneView.hh"
#include "fabric/core/Spatial.hh"
#include "fabric/core/Temporal.hh"
#include "fabric/parser/ArgumentParser.hh"
#include "fabric/utils/Profiler.hh"

#include <bgfx/bgfx.h>
#include <bgfx/platform.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_properties.h>

#include <chrono>
#include <cmath>
#include <iostream>

namespace {

bgfx::PlatformData getPlatformData(SDL_Window* window) {
    bgfx::PlatformData pd{};
    SDL_PropertiesID props = SDL_GetWindowProperties(window);

#if defined(SDL_PLATFORM_WIN32)
    pd.nwh = SDL_GetPointerProperty(
        props, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);
#elif defined(SDL_PLATFORM_MACOS)
    pd.nwh = SDL_GetPointerProperty(
        props, SDL_PROP_WINDOW_COCOA_WINDOW_POINTER, nullptr);
#elif defined(SDL_PLATFORM_LINUX)
    void* wl = SDL_GetPointerProperty(
        props, SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER, nullptr);
    if (wl) {
        pd.ndt = SDL_GetPointerProperty(
            props, SDL_PROP_WINDOW_WAYLAND_DISPLAY_POINTER, nullptr);
        pd.nwh = wl;
        pd.type = bgfx::NativeWindowHandleType::Wayland;
    } else {
        pd.ndt = SDL_GetPointerProperty(
            props, SDL_PROP_WINDOW_X11_DISPLAY_POINTER, nullptr);
        pd.nwh = reinterpret_cast<void*>(static_cast<uintptr_t>(
            SDL_GetNumberProperty(
                props, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0)));
    }
#endif

    return pd;
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
        std::cout << fabric::APP_NAME << " version " << fabric::APP_VERSION
                  << std::endl;
        fabric::log::shutdown();
        return 0;
    }

    if (argParser.hasArgument("--help")) {
        std::cout << "Usage: " << fabric::APP_EXECUTABLE_NAME << " [options]"
                  << std::endl;
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

        SDL_Window* window = SDL_CreateWindow(
            fabric::APP_NAME, kWindowWidth, kWindowHeight,
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

        bgfx::setViewClear(
            0, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x303030ff, 1.0f, 0);
        bgfx::setViewRect(
            0, 0, 0, static_cast<uint16_t>(pw), static_cast<uint16_t>(ph));

        FABRIC_LOG_INFO("bgfx renderer: {}",
            bgfx::getRendererName(bgfx::getRendererType()));

        fabric::async::init();

        // Interactive subsystem setup
        fabric::EventDispatcher dispatcher;
        fabric::InputManager inputManager(dispatcher);

        // WASD + space/shift movement bindings
        inputManager.bindKey("move_forward", SDLK_W);
        inputManager.bindKey("move_backward", SDLK_S);
        inputManager.bindKey("move_left", SDLK_A);
        inputManager.bindKey("move_right", SDLK_D);
        inputManager.bindKey("move_up", SDLK_SPACE);
        inputManager.bindKey("move_down", SDLK_LSHIFT);

        // Time control bindings
        inputManager.bindKey("time_pause", SDLK_P);
        inputManager.bindKey("time_faster", SDLK_EQUALS);
        inputManager.bindKey("time_slower", SDLK_MINUS);

        auto& timeline = fabric::Timeline::instance();

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
            if (scale > 4.0) scale = 4.0;
            timeline.setGlobalTimeScale(scale);
            FABRIC_LOG_INFO("Time scale: {:.2f}", timeline.getGlobalTimeScale());
        });

        dispatcher.addEventListener("time_slower", [&timeline](fabric::Event&) {
            double scale = timeline.getGlobalTimeScale() - 0.25;
            if (scale < 0.25) scale = 0.25;
            timeline.setGlobalTimeScale(scale);
            FABRIC_LOG_INFO("Time scale: {:.2f}", timeline.getGlobalTimeScale());
        });

        // Camera setup
        fabric::Camera camera;
        bool homogeneousNdc = bgfx::getCaps()->homogeneousDepth;
        float aspect = static_cast<float>(pw) / static_cast<float>(ph);
        camera.setPerspective(60.0f, aspect, 0.1f, 1000.0f, homogeneousNdc);

        fabric::Transform<float> cameraTransform;
        cameraTransform.setPosition(fabric::Vector3<float, fabric::Space::World>(0.0f, 0.0f, -5.0f));
        camera.updateView(cameraTransform);

        // Scene setup
        fabric::Scene scene;
        fabric::SceneView sceneView(0, camera, *scene.getRoot());

        // Camera control state
        constexpr float kMoveSpeed = 5.0f;
        constexpr float kMouseSensitivity = 0.002f;
        float cameraYaw = 0.0f;
        float cameraPitch = 0.0f;

        FABRIC_LOG_INFO("Interactive systems initialized");

        // Fixed-timestep main loop
        constexpr double kFixedDt = 1.0 / 60.0;
        double accumulator = 0.0;
        auto lastTime = std::chrono::high_resolution_clock::now();
        bool running = true;

        FABRIC_LOG_INFO("Entering main loop");

        while (running) {
            FABRIC_ZONE_SCOPED_N("main_loop");

            auto now = std::chrono::high_resolution_clock::now();
            double frameTime =
                std::chrono::duration<double>(now - lastTime).count();
            lastTime = now;

            if (frameTime > 0.25) frameTime = 0.25;
            accumulator += frameTime;

            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                inputManager.processEvent(event);

                if (event.type == SDL_EVENT_QUIT)
                    running = false;

                if (event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
                    auto w = static_cast<uint32_t>(event.window.data1);
                    auto h = static_cast<uint32_t>(event.window.data2);
                    bgfx::reset(w, h, BGFX_RESET_VSYNC);
                    bgfx::setViewRect(0, 0, 0,
                        static_cast<uint16_t>(w), static_cast<uint16_t>(h));
                    float newAspect = static_cast<float>(w) / static_cast<float>(h);
                    camera.setPerspective(60.0f, newAspect, 0.1f, 1000.0f, homogeneousNdc);
                }
            }

            // Mouse look: apply once per frame (not per fixed step)
            cameraYaw += inputManager.mouseDeltaX() * kMouseSensitivity;
            cameraPitch += inputManager.mouseDeltaY() * kMouseSensitivity;

            constexpr float kMaxPitch = 1.5f; // ~86 degrees
            if (cameraPitch > kMaxPitch) cameraPitch = kMaxPitch;
            if (cameraPitch < -kMaxPitch) cameraPitch = -kMaxPitch;

            // Build camera rotation from yaw (Y axis) then pitch (X axis)
            auto yawQ = fabric::Quaternion<float>::fromAxisAngle(
                fabric::Vector3<float, fabric::Space::World>(0.0f, 1.0f, 0.0f), cameraYaw);
            auto pitchQ = fabric::Quaternion<float>::fromAxisAngle(
                fabric::Vector3<float, fabric::Space::World>(1.0f, 0.0f, 0.0f), cameraPitch);
            auto rotation = yawQ * pitchQ;
            cameraTransform.setRotation(rotation);

            // Derive direction vectors from current rotation
            auto fwd = rotation.rotateVector(
                fabric::Vector3<float, fabric::Space::World>(0.0f, 0.0f, 1.0f));
            auto right = rotation.rotateVector(
                fabric::Vector3<float, fabric::Space::World>(1.0f, 0.0f, 0.0f));
            auto up = fabric::Vector3<float, fabric::Space::World>(0.0f, 1.0f, 0.0f);

            while (accumulator >= kFixedDt) {
                fabric::async::poll();
                timeline.update(kFixedDt);

                // Movement relative to camera orientation
                float step = kMoveSpeed * static_cast<float>(kFixedDt);
                auto pos = cameraTransform.getPosition();

                if (inputManager.isActionActive("move_forward"))  pos = pos + fwd * step;
                if (inputManager.isActionActive("move_backward")) pos = pos - fwd * step;
                if (inputManager.isActionActive("move_right"))    pos = pos + right * step;
                if (inputManager.isActionActive("move_left"))     pos = pos - right * step;
                if (inputManager.isActionActive("move_up"))       pos = pos + up * step;
                if (inputManager.isActionActive("move_down"))     pos = pos - up * step;

                cameraTransform.setPosition(pos);
                accumulator -= kFixedDt;
            }

            camera.updateView(cameraTransform);

            inputManager.beginFrame();

            {
                FABRIC_ZONE_SCOPED_N("render_submit");
                sceneView.render();
                bgfx::frame();
            }

            FABRIC_FRAME_MARK;
        }

        FABRIC_LOG_INFO("Shutting down");

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
