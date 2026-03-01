#pragma once

#include "fabric/core/StateMachine.hh"
#include <array>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

namespace fabric {

enum class AppMode : std::uint8_t {
    Game,
    Paused,
    Console,
    Menu,
    Editor,
};

struct AppModeFlags {
    bool captureMouse;
    bool pauseSimulation;
    bool routeToUI;
    bool routeToGame;
};

static constexpr std::size_t kAppModeCount = 5;

static constexpr std::array<AppModeFlags, kAppModeCount> kAppModeFlagsTable = {{
    {true, false, false, true}, // Game
    {false, true, true, false}, // Paused
    {false, false, true, true}, // Console
    {false, true, true, false}, // Menu
    {false, true, true, false}, // Editor
}};

std::string appModeToString(AppMode mode);

using AppModeCallback = std::function<void(AppMode from, AppMode to)>;

class AppModeManager {
  public:
    AppModeManager();

    void transition(AppMode target);
    void togglePause();

    AppMode current() const;
    AppMode previous() const;

    static const AppModeFlags& flags(AppMode mode);
    static bool isValidTransition(AppMode from, AppMode to);

    std::string addObserver(const AppModeCallback& callback);
    bool removeObserver(const std::string& observerId);

  private:
    struct ObserverEntry {
        std::string id;
        AppModeCallback callback;
    };

    StateMachine<AppMode> sm_;
    AppMode previous_;

    mutable std::mutex observerMutex_;
    std::vector<ObserverEntry> observers_;
};

} // namespace fabric
