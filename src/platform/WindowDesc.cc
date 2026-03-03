#include "fabric/platform/WindowDesc.hh"

#include "fabric/core/Log.hh"

#include <SDL3/SDL.h>
#include <toml++/toml.hpp>

namespace fabric {

uint64_t WindowDesc::toSDLFlags() const {
    uint64_t flags = 0;
    if (vulkan) {
        flags |= SDL_WINDOW_VULKAN;
    }
    if (hidpiEnabled) {
        flags |= SDL_WINDOW_HIGH_PIXEL_DENSITY;
    }
    if (resizable) {
        flags |= SDL_WINDOW_RESIZABLE;
    }
    if (borderless) {
        flags |= SDL_WINDOW_BORDERLESS;
    }
    if (fullscreen) {
        flags |= SDL_WINDOW_FULLSCREEN;
    }
    if (maximized) {
        flags |= SDL_WINDOW_MAXIMIZED;
    }
    if (hidden) {
        flags |= SDL_WINDOW_HIDDEN;
    }
    return flags;
}

void WindowDesc::applyConstraints(SDL_Window* window) const {
    if (window == nullptr) {
        return;
    }
    SDL_SetWindowMinimumSize(window, minWidth, minHeight);
}

SDL_Window* createWindow(const WindowDesc& desc) {
    SDL_Window* win = SDL_CreateWindow(desc.title.c_str(), desc.width, desc.height, desc.toSDLFlags());
    if (win == nullptr) {
        FABRIC_LOG_ERROR("Window creation failed: {}", SDL_GetError());
        return nullptr;
    }

    // Position the window if explicit coordinates were given
    if (desc.posX >= 0 || desc.posY >= 0) {
        int x = desc.posX >= 0 ? desc.posX : static_cast<int>(SDL_WINDOWPOS_CENTERED_DISPLAY(desc.displayIndex));
        int y = desc.posY >= 0 ? desc.posY : static_cast<int>(SDL_WINDOWPOS_CENTERED_DISPLAY(desc.displayIndex));
        SDL_SetWindowPosition(win, x, y);
    }

    desc.applyConstraints(win);

    // fullscreenDesktop handled post-creation (SDL3 uses same flag,
    // different setup path)
    if (desc.fullscreenDesktop) {
        SDL_SetWindowFullscreen(win, true);
    }

    return win;
}

WindowDesc windowDescFromConfig(const toml::table& table) {
    WindowDesc desc;

    if (auto v = table["title"].value<std::string>()) {
        desc.title = *v;
    }
    if (auto v = table["width"].value<int64_t>()) {
        desc.width = static_cast<int32_t>(*v);
    }
    if (auto v = table["height"].value<int64_t>()) {
        desc.height = static_cast<int32_t>(*v);
    }
    if (auto v = table["min_width"].value<int64_t>()) {
        desc.minWidth = static_cast<int32_t>(*v);
    }
    if (auto v = table["min_height"].value<int64_t>()) {
        desc.minHeight = static_cast<int32_t>(*v);
    }
    if (auto v = table["display"].value<int64_t>()) {
        desc.displayIndex = static_cast<uint32_t>(*v);
    }
    if (auto v = table["fullscreen"].value<bool>()) {
        desc.fullscreen = *v;
    }
    if (auto v = table["borderless"].value<bool>()) {
        desc.borderless = *v;
    }
    if (auto v = table["resizable"].value<bool>()) {
        desc.resizable = *v;
    }
    if (auto v = table["hidpi"].value<bool>()) {
        desc.hidpiEnabled = *v;
    }
    if (auto v = table["maximized"].value<bool>()) {
        desc.maximized = *v;
    }
    if (auto v = table["fullscreen_desktop"].value<bool>()) {
        desc.fullscreenDesktop = *v;
    }
    if (auto v = table["hidden"].value<bool>()) {
        desc.hidden = *v;
    }

    return desc;
}

} // namespace fabric
