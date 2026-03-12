#pragma once

#include "fabric/core/SystemBase.hh"
#include "fabric/render/Rendering.hh"
#include "recurse/world/WorldType.hh"
#include <chrono>
#include <functional>
#include <memory>
#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/DataModelHandle.h>
#include <string>
#include <vector>

namespace fabric {
class AppModeManager;
class SystemRegistry;
} // namespace fabric

namespace recurse {
class WorldRegistry;
}

namespace recurse::simulation {
class SimulationGrid;
}

namespace recurse::systems {

class TerrainSystem;
class VoxelSimulationSystem;
class VoxelMeshingSystem;
class CharacterMovementSystem;
class PhysicsGameSystem;
class ChunkPipelineSystem;

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
    WorldSelect, // Quick world type selection (Flat, Natural) - legacy flow
    WorldCreate, // New World screen (name, type, seed)
    WorldList,   // Saved worlds list (load, delete, rename)
    Settings,    // Settings panel (unwired for now)
    Pause,       // In-game pause menu (Resume, Quit to Title, Exit)
    Hidden       // Game is running, menu not visible
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
    using StartGameCallback =
        std::function<void(recurse::WorldType, int64_t seed, const std::string& uuid, bool isNew)>;
    void setStartGameCallback(StartGameCallback cb) { startGameCallback_ = std::move(cb); }

    /// Set the world registry for world management (create, list, delete).
    void setWorldRegistry(recurse::WorldRegistry* reg) { worldRegistry_ = reg; }

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
    void showWorldCreate();
    void showWorldList();
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
    void onNewWorldClicked();
    void onLoadWorldClicked();
    void onCreateWorldClicked();
    void onDeleteWorldClicked(const std::string& uuid);
    void onRenameWorldClicked(const std::string& uuid, const std::string& newName);
    void onOpenFolderClicked(const std::string& uuid);
    void onWorldSelected(const std::string& uuid);

    // Reset all world state (meshes, physics, simulation, terrain)
    void resetWorldState();

    // Enable/disable all 16 world-dependent systems via SystemRegistry.
    // Called at init (disable), world creation (enable), world destruction (disable).
    void setWorldSystemsEnabled(bool enabled);

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
    ChunkPipelineSystem* chunkPipeline_ = nullptr;
    fabric::AppModeManager* appModeManager_ = nullptr;
    fabric::SystemRegistry* registry_ = nullptr;

    // World management
    recurse::WorldRegistry* worldRegistry_ = nullptr;

    // Data model bindings
    std::string titleText_ = "RECURSE";
    std::string versionText_ = "v0.1.0";

    // New World screen bindings
    std::string newWorldName_ = "New World";
    std::string newWorldType_ = "Natural";
    std::string newWorldSeed_;

    // World List screen state
    std::string selectedWorldUUID_;
    std::string renamingWorldUUID_;
    std::string deletingWorldUUID_;
};

} // namespace recurse::systems
