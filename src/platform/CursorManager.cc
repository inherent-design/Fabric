#include "fabric/platform/CursorManager.hh"

#include <SDL3/SDL.h>

namespace fabric {

CursorManager::CursorManager(SDL_Window* window) : window_(window) {}

void CursorManager::setMode(CursorMode mode) {
    if (mode_ == mode) {
        return;
    }
    mode_ = mode;

    switch (mode) {
        case CursorMode::Normal:
            SDL_SetWindowRelativeMouseMode(window_, false);
            SDL_SetWindowMouseGrab(window_, false);
            SDL_ShowCursor();
            break;
        case CursorMode::Captured:
            SDL_SetWindowRelativeMouseMode(window_, true);
            break;
        case CursorMode::Confined:
            SDL_SetWindowRelativeMouseMode(window_, false);
            SDL_SetWindowMouseGrab(window_, true);
            SDL_ShowCursor();
            break;
    }
}

CursorMode CursorManager::currentMode() const {
    return mode_;
}

void CursorManager::applyCaptureFlag(bool capture) {
    setMode(capture ? CursorMode::Captured : CursorMode::Normal);
}

} // namespace fabric
