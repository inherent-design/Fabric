#pragma once

#include "fabric/core/SystemBase.hh"
#include "fabric/core/SystemPhase.hh"
#include "fabric/platform/WindowDesc.hh"
#include "fabric/utils/ErrorHandling.hh"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <toml++/toml.hpp>
#include <tuple>
#include <typeindex>
#include <vector>

namespace fabric {

struct AppContext;

/// Application descriptor. Built by the application's main(), passed
/// to FabricApp::run(). No inheritance, no virtuals.
struct FabricAppDesc {
    // Application identity
    std::string name = "Fabric";
    std::string configPath; // App-layer config, e.g., "recurse.toml"

    // Window configuration (fallback if no TOML config found)
    WindowDesc windowDesc;

    // App-layer config defaults (injected between compiled defaults and engine TOML).
    // Example: Recurse sets borderless fullscreen here so it overrides engine defaults
    // but can still be overridden by fabric.toml, recurse.toml, user.toml, or CLI.
    toml::table configDefaults;

    /// Register a system for deferred construction inside FabricApp::run().
    /// Constructor args are captured in a lambda; the system is instantiated
    /// after engine infrastructure is ready.
    template <typename T, typename... Args> void registerSystem(SystemPhase phase, Args&&... args) {
        auto tid = std::type_index(typeid(T));
        for (const auto& reg : systemRegistrations_) {
            if (reg.typeId == tid) {
                throwError("Duplicate system type registration in FabricAppDesc");
            }
        }
        systemRegistrations_.push_back(
            {phase,
             [args_tuple = std::make_tuple(std::forward<Args>(args)...)]() mutable -> std::unique_ptr<SystemBase> {
                 return std::apply([](auto&&... a) { return std::make_unique<T>(std::forward<decltype(a)>(a)...); },
                                   std::move(args_tuple));
             },
             tid});
    }

    // Lifecycle callbacks (all optional, receive AppContext&)
    std::function<void(AppContext&)> onInit;
    std::function<void(AppContext&)> onShutdown;

    // Window/focus event callbacks
    std::function<void(AppContext&)> onFocusGained;
    std::function<void(AppContext&)> onFocusLost;
    std::function<void(AppContext&, uint32_t width, uint32_t height)> onResize;

    // When true, skip SDL/bgfx init and main loop (safe for tests).
    // Real executables set this to false.
    bool headless = true;

    // Internal: used by FabricApp::run() to instantiate systems.
    struct SystemRegistration {
        SystemPhase phase;
        std::function<std::unique_ptr<SystemBase>()> factory;
        std::type_index typeId = std::type_index(typeid(void));
    };
    std::vector<SystemRegistration> systemRegistrations_;
};

} // namespace fabric
