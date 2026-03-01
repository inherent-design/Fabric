#include "fabric/core/AppModeManager.hh"
#include "fabric/core/Log.hh"
#include "fabric/utils/ErrorHandling.hh"
#include "fabric/utils/Utils.hh"

#include <set>

namespace fabric {

std::string appModeToString(AppMode mode) {
    switch (mode) {
        case AppMode::Game:
            return "Game";
        case AppMode::Paused:
            return "Paused";
        case AppMode::Console:
            return "Console";
        case AppMode::Menu:
            return "Menu";
        case AppMode::Editor:
            return "Editor";
        default:
            return "Unknown";
    }
}

namespace {

// Strict transition table: only key-triggered transitions are valid.
// Game is the hub -- all overlay modes transit through Game.
// Overlay->Paused is allowed for Esc from any overlay.
const auto& appModeTransitions() {
    static const std::set<std::pair<AppMode, AppMode>> t = {
        // Esc: Game <-> Paused
        {AppMode::Game, AppMode::Paused},
        {AppMode::Paused, AppMode::Game},
        // Backtick: Game <-> Console
        {AppMode::Game, AppMode::Console},
        {AppMode::Console, AppMode::Game},
        // F7: Game <-> Editor
        {AppMode::Game, AppMode::Editor},
        {AppMode::Editor, AppMode::Game},
        // F11: Game <-> Menu
        {AppMode::Game, AppMode::Menu},
        {AppMode::Menu, AppMode::Game},
        // Esc from overlay modes -> Paused
        {AppMode::Console, AppMode::Paused},
        {AppMode::Editor, AppMode::Paused},
        {AppMode::Menu, AppMode::Paused},
    };
    return t;
}

} // anonymous namespace

AppModeManager::AppModeManager() : sm_(AppMode::Game, appModeToString), previous_(AppMode::Game) {
    for (const auto& [from, to] : appModeTransitions()) {
        sm_.addTransition(from, to);
    }
}

void AppModeManager::transition(AppMode target) {
    auto old = sm_.getState();
    if (old == target) {
        return;
    }
    sm_.setState(target);
    previous_ = old;
    FABRIC_LOG_INFO("AppMode: {} -> {}", appModeToString(old), appModeToString(target));

    std::vector<AppModeCallback> callbacks;
    {
        std::lock_guard<std::mutex> lock(observerMutex_);
        callbacks.reserve(observers_.size());
        for (const auto& entry : observers_) {
            callbacks.push_back(entry.callback);
        }
    }
    for (const auto& cb : callbacks) {
        try {
            cb(old, target);
        } catch (const std::exception& e) {
            FABRIC_LOG_ERROR("Exception in AppMode observer: {}", e.what());
        }
    }
}

void AppModeManager::togglePause() {
    auto mode = sm_.getState();
    if (mode == AppMode::Game) {
        transition(AppMode::Paused);
    } else if (mode == AppMode::Paused) {
        transition(AppMode::Game);
    }
}

AppMode AppModeManager::current() const {
    return sm_.getState();
}

AppMode AppModeManager::previous() const {
    return previous_;
}

const AppModeFlags& AppModeManager::flags(AppMode mode) {
    auto idx = static_cast<std::size_t>(mode);
    if (idx >= kAppModeCount) {
        throwError("Invalid AppMode: " + std::to_string(idx));
    }
    return kAppModeFlagsTable[idx];
}

bool AppModeManager::isValidTransition(AppMode from, AppMode to) {
    if (from == to) {
        return true;
    }
    return appModeTransitions().count({from, to}) > 0;
}

std::string AppModeManager::addObserver(const AppModeCallback& callback) {
    if (!callback) {
        throwError("AppMode observer callback cannot be null");
    }
    std::lock_guard<std::mutex> lock(observerMutex_);
    auto id = Utils::generateUniqueId("appmode_");
    observers_.push_back({id, callback});
    return id;
}

bool AppModeManager::removeObserver(const std::string& observerId) {
    std::lock_guard<std::mutex> lock(observerMutex_);
    auto it = std::find_if(observers_.begin(), observers_.end(),
                           [&observerId](const ObserverEntry& e) { return e.id == observerId; });
    if (it != observers_.end()) {
        observers_.erase(it);
        return true;
    }
    return false;
}

} // namespace fabric
