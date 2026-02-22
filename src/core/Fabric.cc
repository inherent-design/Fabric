#include "fabric/core/Async.hh"
#include "fabric/core/Constants.g.hh"
#include "fabric/core/Log.hh"
#include "fabric/parser/ArgumentParser.hh"
#include "fabric/utils/Profiler.hh"

#include <bgfx/bgfx.h>
#include <bgfx/platform.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_properties.h>

#include <chrono>
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
                if (event.type == SDL_EVENT_QUIT)
                    running = false;

                if (event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
                    auto w = static_cast<uint32_t>(event.window.data1);
                    auto h = static_cast<uint32_t>(event.window.data2);
                    bgfx::reset(w, h, BGFX_RESET_VSYNC);
                    bgfx::setViewRect(0, 0, 0,
                        static_cast<uint16_t>(w), static_cast<uint16_t>(h));
                }
            }

            while (accumulator >= kFixedDt) {
                fabric::async::poll();
                accumulator -= kFixedDt;
            }

            bgfx::touch(0);

            FABRIC_FRAME_MARK;
            bgfx::frame();
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
