#pragma once

#include <cstdint>

struct SDL_Window;

namespace fabric {

enum class CursorMode : uint8_t {
    Normal,   // visible, absolute positioning
    Captured, // hidden, relative mode (FPS camera)
    Confined  // visible, confined to window bounds
};

class CursorManager {
  public:
    explicit CursorManager(SDL_Window* window);

    void setMode(CursorMode mode);
    CursorMode currentMode() const;

    // Maps a boolean capture flag to Normal/Captured modes.
    // Intended for AppModeFlags::captureMouse integration.
    void applyCaptureFlag(bool capture);

  private:
    SDL_Window* window_;
    CursorMode mode_ = CursorMode::Normal;
};

} // namespace fabric
