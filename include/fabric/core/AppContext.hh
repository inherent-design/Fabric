#pragma once

#include "fabric/core/Event.hh"
#include "fabric/core/Temporal.hh"
#include "fabric/ecs/ECS.hh"

struct SDL_Window;

namespace Rml {
class Context;
} // namespace Rml

namespace fabric {

// Forward declarations to keep AppContext lightweight
class ResourceHub;
class AssetRegistry;
class SystemRegistry;
class ConfigManager;
class InputSystem;
struct RuntimeState;
class PlatformInfo;
struct RenderCaps;
class AppModeManager;
class CursorManager;
class InputManager;
class InputRouter;
class Camera;
class SceneView;

/// Non-owning view into engine services. Constructed once at startup,
/// passed to systems via init/update/render. Immutable after construction.
///
/// Required members use references (always present).
/// Optional members use raw pointers (nullptr when unavailable, e.g., headless mode).
struct AppContext {
    // Required services (always present)
    World& world;
    Timeline& timeline;
    EventDispatcher& dispatcher;
    ResourceHub& resourceHub;
    AssetRegistry& assetRegistry;
    SystemRegistry& systemRegistry;
    ConfigManager& configManager;

    // Optional services (nullptr when unavailable)
    InputSystem* inputSystem = nullptr;
    RuntimeState* runtimeState = nullptr;
    PlatformInfo* platformInfo = nullptr;
    RenderCaps* renderCaps = nullptr;
    AppModeManager* appModeManager = nullptr;
    SDL_Window* window = nullptr;
    CursorManager* cursorManager = nullptr;
    InputManager* inputManager = nullptr;
    InputRouter* inputRouter = nullptr;
    Camera* camera = nullptr;
    SceneView* sceneView = nullptr;
    Rml::Context* rmlContext = nullptr;
};

} // namespace fabric
