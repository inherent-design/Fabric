#pragma once

#include "fabric/core/Rendering.hh"
#include "fabric/core/SystemBase.hh"
#include <chrono>
#include <functional>
#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/DataModelHandle.h>
#include <string>
#include <vector>

namespace fabric {
class AppModeManager;
}

namespace fabric::simulation {
class SimulationGrid;
}

namespace recurse::systems {

class TerrainSystem;
class VoxelSimulationSystem;
class VoxelMeshingSystem;
class CharacterMovementSystem;
class PhysicsGameSystem;

/// Splash screen media entry
struct SplashEntry {
    std::string type;   // "image", "video", "scene"
    std::string source; // File path or scene identifier
    float durationSec;  // Time to display before next entry
};

/// Menu state machine
enum class MenuState {
    Splash,      // Showing splash screens
    TitleScreen, // Main menu (Start, Settings, Quit)
    WorldSelect, // World type selection (Flat, Minecraft)
    Settings,    // Settings panel (unwired for now)
    Pause,       // In-game pause menu (Resume, Quit to Title, Exit)
    Hidden       // Game is running, menu not visible
};

/// World type selection
enum class WorldType {
    Flat,     // FlatWorldGenerator - stone below y=32
    Minecraft // MinecraftNoiseGenerator - procedural terrain
};

/// Main menu system with splash screen support.
/// - Splash: array of media + timeout, advances through entries
/// - Title: Start, Settings, Quit buttons
/// - WorldSelect: Flat, Minecraft buttons -> launches game
class MainMenuSystem : public fabric::System<MainMenuSystem> {
  public:
    MainMenuSystem();
    ~MainMenuSystem() override;

    void doInit(fabric::AppContext& ctx) override;
    void doShutdown() override;
    void update(fabric::AppContext& ctx, float dt) override;
    void render(fabric::AppContext& ctx) override;
    void configureDependencies() override;

    // State queries
    MenuState menuState() const { return menuState_; }
    bool isVisible() const { return menuState_ != MenuState::Hidden; }

    // Callbacks for game integration
    using StartGameCallback = std::function<void(WorldType)>;
    void setStartGameCallback(StartGameCallback cb) { startGameCallback_ = std::move(cb); }

    // Splash configuration
    void setSplashEntries(std::vector<SplashEntry> entries);
    void skipSplash(); // Skip to title screen

  private:
    // RmlUi setup
    void initRmlDataModel();
    void showDocument(const std::string& rmlPath);
    void hideCurrentDocument();

    // State transitions
    void transitionTo(MenuState newState);
    void advanceSplash(float dt);
    void showTitleScreen();
    void showWorldSelect();
    void showSettings();
    void showPause();
    void hideMenu();

    // Button handlers (called from RmlUi)
    void onStartClicked();
    void onSettingsClicked();
    void onQuitClicked();
    void onFlatWorldClicked();
    void onMinecraftWorldClicked();
    void onBackClicked();
    void onResumeClicked();
    void onQuitToTitleClicked();
    void onExitToDesktopClicked();

    // Reset all world state (meshes, physics, simulation, terrain)
    void resetWorldState();

    // Members
    Rml::Context* rmlContext_ = nullptr;
    Rml::ElementDocument* currentDocument_ = nullptr;
    Rml::DataModelHandle dataModelHandle_;

    MenuState menuState_ = MenuState::Splash;
    std::vector<SplashEntry> splashEntries_;
    size_t currentSplashIndex_ = 0;
    float splashTimer_ = 0.0f;

    StartGameCallback startGameCallback_;
    TerrainSystem* terrain_ = nullptr;
    VoxelSimulationSystem* voxelSim_ = nullptr;
    VoxelMeshingSystem* voxelMesh_ = nullptr;
    CharacterMovementSystem* characterMovement_ = nullptr;
    PhysicsGameSystem* physics_ = nullptr;
    fabric::AppModeManager* appModeManager_ = nullptr;

    // Data model bindings
    std::string titleText_ = "RECURSE";
    std::string versionText_ = "v0.1.0";
};

} // namespace recurse::systems
