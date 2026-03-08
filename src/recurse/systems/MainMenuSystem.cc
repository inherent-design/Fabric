#include "recurse/systems/MainMenuSystem.hh"
#include "recurse/character/GameConstants.hh"

#include "fabric/core/AppContext.hh"
#include "fabric/core/AppModeManager.hh"
#include "fabric/core/Event.hh"
#include "fabric/core/Log.hh"
#include "fabric/core/SystemRegistry.hh"
#include "fabric/world/MinecraftNoiseGenerator.hh"
#include "recurse/systems/CharacterMovementSystem.hh"
#include "recurse/systems/PhysicsGameSystem.hh"
#include "recurse/systems/TerrainSystem.hh"
#include "recurse/systems/VoxelMeshingSystem.hh"
#include "recurse/systems/VoxelSimulationSystem.hh"
#include "recurse/world/TestWorldGenerator.hh"

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/EventListener.h>

#include <chrono>
#include <SDL3/SDL.h>

namespace recurse::systems {

namespace {

/// Minecraft-style world generator conforming to recurse::WorldGenerator interface
class MinecraftWorldGenerator : public WorldGenerator {
  public:
    explicit MinecraftWorldGenerator(const fabric::world::NoiseGenConfig& config) : noiseGen_(config) {}

    void generate(recurse::simulation::SimulationGrid& grid, int cx, int cy, int cz) override {
        noiseGen_.generate(grid, recurse::simulation::ChunkPos{cx, cy, cz});
    }

    std::string name() const override { return "MinecraftWorldGenerator"; }

  private:
    fabric::world::MinecraftNoiseGenerator noiseGen_;
};

/// RmlUi event listener for button clicks
class MenuButtonListener : public Rml::EventListener {
  public:
    using Callback = std::function<void()>;

    MenuButtonListener(Callback cb) : callback_(std::move(cb)) {}

    void ProcessEvent(Rml::Event& event) override {
        FABRIC_LOG_INFO("MenuButtonListener: Click event on element '{}'", event.GetCurrentElement()->GetId().c_str());
        callback_();
    }

    void OnDetach(Rml::Element* /*element*/) override { delete this; }

  private:
    Callback callback_;
};

} // namespace

MainMenuSystem::MainMenuSystem() = default;
MainMenuSystem::~MainMenuSystem() = default;

void MainMenuSystem::doInit(fabric::AppContext& ctx) {
    FABRIC_LOG_INFO("MainMenuSystem::init starting");

    terrain_ = ctx.systemRegistry.get<TerrainSystem>();
    voxelSim_ = ctx.systemRegistry.get<VoxelSimulationSystem>();
    voxelMesh_ = ctx.systemRegistry.get<VoxelMeshingSystem>();
    characterMovement_ = ctx.systemRegistry.get<CharacterMovementSystem>();
    physics_ = ctx.systemRegistry.get<PhysicsGameSystem>();
    appModeManager_ = ctx.appModeManager;
    rmlContext_ = ctx.rmlContext;

    FABRIC_LOG_INFO("MainMenuSystem: rmlContext={}, appModeManager={}", (void*)rmlContext_, (void*)appModeManager_);

    if (!rmlContext_) {
        FABRIC_LOG_ERROR("MainMenuSystem: No RmlUi context available");
        return;
    }

    initRmlDataModel();

    // Set up start game callback
    startGameCallback_ = [this](WorldType type) {
        if (!terrain_) {
            FABRIC_LOG_ERROR("MainMenuSystem: Cannot start game - no TerrainSystem");
            return;
        }

        // Generate a random seed
        int seed = static_cast<int>(std::chrono::steady_clock::now().time_since_epoch().count());

        // Set world generator based on selection
        if (type == WorldType::Minecraft) {
            auto config = fabric::world::NoiseGenConfig{};
            config.seed = seed;
            terrain_->setWorldGenerator(std::make_unique<MinecraftWorldGenerator>(config));
            FABRIC_LOG_INFO("MainMenu: Using MinecraftNoiseGenerator (seed={})", seed);
        } else {
            terrain_->setWorldGenerator(std::make_unique<FlatWorldGenerator>());
            FABRIC_LOG_INFO("MainMenu: Using FlatWorldGenerator");
        }

        // Generate the world in VoxelSimulationSystem
        if (voxelSim_) {
            voxelSim_->generateInitialWorld();
        }

        // Reset player position to spawn point (above generated terrain)
        if (characterMovement_) {
            // Flat world ground is at y=32, spawn 10 units above
            float spawnY = (type == WorldType::Flat) ? 42.0f : 64.0f;
            characterMovement_->setPlayerPosition(fabric::Vec3f(K_DEFAULT_SPAWN_X, spawnY, K_DEFAULT_SPAWN_Z));
            FABRIC_LOG_INFO("MainMenu: Player position reset to ({}, {}, {})", K_DEFAULT_SPAWN_X, spawnY,
                            K_DEFAULT_SPAWN_Z);
        }

        // Transition to Game mode
        if (appModeManager_) {
            appModeManager_->transition(fabric::AppMode::Game);
        }
    };

    // AppMode observer: show/hide pause menu based on mode transitions
    if (appModeManager_) {
        appModeManager_->addObserver([this](fabric::AppMode from, fabric::AppMode to) {
            FABRIC_LOG_DEBUG("MainMenuSystem AppMode observer: {} -> {}", fabric::appModeToString(from),
                             fabric::appModeToString(to));

            if (to == fabric::AppMode::Paused && menuState_ == MenuState::Hidden) {
                // Entering pause mode from game - show pause menu
                transitionTo(MenuState::Pause);
            } else if (to == fabric::AppMode::Game && menuState_ == MenuState::Pause) {
                // Resuming game - hide pause menu
                transitionTo(MenuState::Hidden);
            } else if (to == fabric::AppMode::Menu && menuState_ != MenuState::TitleScreen) {
                // Returning to title - reset world state and show main menu
                resetWorldState();
                transitionTo(MenuState::TitleScreen);
            }
        });
    }

    // Start with splash (or skip if empty)
    if (splashEntries_.empty()) {
        transitionTo(MenuState::TitleScreen);
    } else {
        transitionTo(MenuState::Splash);
    }

    FABRIC_LOG_INFO("MainMenuSystem initialized");
}

void MainMenuSystem::initRmlDataModel() {
    if (!rmlContext_)
        return;

    auto constructor = rmlContext_->CreateDataModel("main_menu");
    if (!constructor) {
        // Model may already exist from another system
        return;
    }

    constructor.Bind("title_text", &titleText_);
    constructor.Bind("version_text", &versionText_);

    dataModelHandle_ = constructor.GetModelHandle();
}

void MainMenuSystem::doShutdown() {
    hideCurrentDocument();
    if (rmlContext_ && dataModelHandle_) {
        rmlContext_->RemoveDataModel("main_menu");
    }
    rmlContext_ = nullptr;
    currentDocument_ = nullptr;
    terrain_ = nullptr;
    voxelSim_ = nullptr;
    voxelMesh_ = nullptr;
    characterMovement_ = nullptr;
    physics_ = nullptr;
    appModeManager_ = nullptr;
    FABRIC_LOG_INFO("MainMenuSystem shutdown");
}

void MainMenuSystem::update(fabric::AppContext& /*ctx*/, float dt) {
    if (menuState_ == MenuState::Splash) {
        advanceSplash(dt);
    }
}

void MainMenuSystem::render(fabric::AppContext& ctx) {
    // RmlUi renders via its own pipeline, just ensure context updates
    if (rmlContext_) {
        rmlContext_->Update();
    }
}

void MainMenuSystem::configureDependencies() {
    after<TerrainSystem>();
    after<VoxelSimulationSystem>();
    after<VoxelMeshingSystem>();
    after<CharacterMovementSystem>();
    after<PhysicsGameSystem>();
}

void MainMenuSystem::setSplashEntries(std::vector<SplashEntry> entries) {
    splashEntries_ = std::move(entries);
    currentSplashIndex_ = 0;
    splashTimer_ = 0.0f;
}

void MainMenuSystem::skipSplash() {
    if (menuState_ == MenuState::Splash) {
        transitionTo(MenuState::TitleScreen);
    }
}

void MainMenuSystem::transitionTo(MenuState newState) {
    if (menuState_ == newState)
        return;

    MenuState prevState = menuState_;
    menuState_ = newState;

    FABRIC_LOG_DEBUG("MainMenuSystem: {} -> {}", static_cast<int>(prevState), static_cast<int>(newState));

    switch (newState) {
        case MenuState::Splash:
            currentSplashIndex_ = 0;
            splashTimer_ = 0.0f;
            break;
        case MenuState::TitleScreen:
            showTitleScreen();
            break;
        case MenuState::WorldSelect:
            showWorldSelect();
            break;
        case MenuState::Settings:
            showSettings();
            break;
        case MenuState::Pause:
            showPause();
            break;
        case MenuState::Hidden:
            hideMenu();
            break;
    }
}

void MainMenuSystem::advanceSplash(float dt) {
    if (splashEntries_.empty()) {
        transitionTo(MenuState::TitleScreen);
        return;
    }

    splashTimer_ += dt;
    if (splashTimer_ >= splashEntries_[currentSplashIndex_].durationSec) {
        splashTimer_ = 0.0f;
        currentSplashIndex_++;

        if (currentSplashIndex_ >= splashEntries_.size()) {
            transitionTo(MenuState::TitleScreen);
        }
    }
}

void MainMenuSystem::showTitleScreen() {
    FABRIC_LOG_INFO("MainMenuSystem::showTitleScreen called");
    showDocument("assets/ui/main_menu.rml");

    if (currentDocument_) {
        // Wire up button handlers
        auto* startBtn = currentDocument_->GetElementById("btn_start");
        if (startBtn) {
            FABRIC_LOG_INFO("MainMenuSystem: Found btn_start, wiring click handler");
            startBtn->AddEventListener(Rml::EventId::Click, new MenuButtonListener([this]() { onStartClicked(); }),
                                       true);
        } else {
            FABRIC_LOG_WARN("MainMenuSystem: btn_start not found!");
        }

        auto* settingsBtn = currentDocument_->GetElementById("btn_settings");
        if (settingsBtn) {
            FABRIC_LOG_INFO("MainMenuSystem: Found btn_settings, wiring click handler");
            settingsBtn->AddEventListener(Rml::EventId::Click,
                                          new MenuButtonListener([this]() { onSettingsClicked(); }), true);
        }

        auto* quitBtn = currentDocument_->GetElementById("btn_quit");
        if (quitBtn) {
            FABRIC_LOG_INFO("MainMenuSystem: Found btn_quit, wiring click handler");
            quitBtn->AddEventListener(Rml::EventId::Click, new MenuButtonListener([this]() { onQuitClicked(); }), true);
        }
    }
}

void MainMenuSystem::showWorldSelect() {
    showDocument("assets/ui/world_select.rml");

    if (currentDocument_) {
        auto* flatBtn = currentDocument_->GetElementById("btn_flat");
        if (flatBtn) {
            flatBtn->AddEventListener(Rml::EventId::Click, new MenuButtonListener([this]() { onFlatWorldClicked(); }),
                                      true);
        }

        auto* minecraftBtn = currentDocument_->GetElementById("btn_minecraft");
        if (minecraftBtn) {
            minecraftBtn->AddEventListener(Rml::EventId::Click,
                                           new MenuButtonListener([this]() { onMinecraftWorldClicked(); }), true);
        }

        auto* backBtn = currentDocument_->GetElementById("btn_back");
        if (backBtn) {
            backBtn->AddEventListener(Rml::EventId::Click, new MenuButtonListener([this]() { onBackClicked(); }), true);
        }
    }
}

void MainMenuSystem::showSettings() {
    showDocument("assets/ui/settings.rml");

    if (currentDocument_) {
        auto* backBtn = currentDocument_->GetElementById("btn_back");
        if (backBtn) {
            backBtn->AddEventListener(Rml::EventId::Click, new MenuButtonListener([this]() { onBackClicked(); }), true);
        }
    }
}

void MainMenuSystem::showPause() {
    FABRIC_LOG_INFO("MainMenuSystem::showPause called");
    showDocument("assets/ui/pause_menu.rml");

    if (!currentDocument_) {
        FABRIC_LOG_ERROR("MainMenuSystem::showPause: Failed to load pause_menu.rml");
        return;
    }

    auto* resumeBtn = currentDocument_->GetElementById("btn_resume");
    if (resumeBtn) {
        FABRIC_LOG_INFO("MainMenuSystem: Found btn_resume, wiring click handler");
        resumeBtn->AddEventListener(Rml::EventId::Click, new MenuButtonListener([this]() { onResumeClicked(); }), true);
    } else {
        FABRIC_LOG_WARN("MainMenuSystem::showPause: btn_resume not found!");
    }

    auto* titleBtn = currentDocument_->GetElementById("btn_quit_to_title");
    if (titleBtn) {
        FABRIC_LOG_INFO("MainMenuSystem: Found btn_quit_to_title, wiring click handler");
        titleBtn->AddEventListener(Rml::EventId::Click, new MenuButtonListener([this]() { onQuitToTitleClicked(); }),
                                   true);
    } else {
        FABRIC_LOG_WARN("MainMenuSystem::showPause: btn_quit_to_title not found!");
    }

    auto* exitBtn = currentDocument_->GetElementById("btn_exit_to_desktop");
    if (exitBtn) {
        FABRIC_LOG_INFO("MainMenuSystem: Found btn_exit_to_desktop, wiring click handler");
        exitBtn->AddEventListener(Rml::EventId::Click, new MenuButtonListener([this]() { onExitToDesktopClicked(); }),
                                  true);
    } else {
        FABRIC_LOG_WARN("MainMenuSystem::showPause: btn_exit_to_desktop not found!");
    }
}

void MainMenuSystem::hideMenu() {
    hideCurrentDocument();
}

void MainMenuSystem::showDocument(const std::string& rmlPath) {
    hideCurrentDocument();

    if (!rmlContext_) {
        FABRIC_LOG_ERROR("MainMenuSystem: Cannot load document - no RmlUi context");
        return;
    }

    currentDocument_ = rmlContext_->LoadDocument(rmlPath);
    if (currentDocument_) {
        currentDocument_->Show();
        FABRIC_LOG_INFO("MainMenuSystem: Loaded and showing {}", rmlPath);
    } else {
        FABRIC_LOG_ERROR("MainMenuSystem: Failed to load {}", rmlPath);
    }
}

void MainMenuSystem::hideCurrentDocument() {
    if (currentDocument_) {
        currentDocument_->Close();
        currentDocument_ = nullptr;
    }
}

void MainMenuSystem::onStartClicked() {
    FABRIC_LOG_INFO("MainMenu: Start clicked");
    transitionTo(MenuState::WorldSelect);
}

void MainMenuSystem::onSettingsClicked() {
    FABRIC_LOG_INFO("MainMenu: Settings clicked");
    transitionTo(MenuState::Settings);
}

void MainMenuSystem::onQuitClicked() {
    FABRIC_LOG_INFO("MainMenu: Quit clicked");
    // Signal app to quit
    SDL_Event quitEvent;
    quitEvent.type = SDL_EVENT_QUIT;
    SDL_PushEvent(&quitEvent);
}

void MainMenuSystem::onFlatWorldClicked() {
    FABRIC_LOG_INFO("MainMenu: Flat world selected");
    if (startGameCallback_) {
        startGameCallback_(WorldType::Flat);
    }
    transitionTo(MenuState::Hidden);
}

void MainMenuSystem::onMinecraftWorldClicked() {
    FABRIC_LOG_INFO("MainMenu: Minecraft world selected");
    if (startGameCallback_) {
        startGameCallback_(WorldType::Minecraft);
    }
    transitionTo(MenuState::Hidden);
}

void MainMenuSystem::onBackClicked() {
    FABRIC_LOG_DEBUG("MainMenu: Back clicked");
    transitionTo(MenuState::TitleScreen);
}

void MainMenuSystem::onResumeClicked() {
    FABRIC_LOG_INFO("MainMenu: Resume clicked");
    // Resume the game - transition back to Game mode
    transitionTo(MenuState::Hidden);
    if (appModeManager_) {
        appModeManager_->transition(fabric::AppMode::Game);
    }
}

void MainMenuSystem::onQuitToTitleClicked() {
    FABRIC_LOG_INFO("MainMenu: Quit to Title clicked");
    resetWorldState();
    transitionTo(MenuState::TitleScreen);
    if (appModeManager_) {
        appModeManager_->transition(fabric::AppMode::Menu);
    }
}

void MainMenuSystem::resetWorldState() {
    // Reset all world state before returning to title screen
    // Order: meshes -> physics -> simulation
    if (voxelMesh_) {
        voxelMesh_->clearAllMeshes();
        FABRIC_LOG_DEBUG("MainMenu: Cleared GPU meshes");
    }
    if (physics_) {
        physics_->clearAllCollisions();
        FABRIC_LOG_DEBUG("MainMenu: Cleared physics collisions");
    }
    if (voxelSim_) {
        voxelSim_->resetWorld();
        FABRIC_LOG_DEBUG("MainMenu: Reset voxel simulation");
    }
}

void MainMenuSystem::onExitToDesktopClicked() {
    FABRIC_LOG_INFO("MainMenu: Exit to Desktop clicked");
    // Signal app to quit
    SDL_Event quitEvent;
    quitEvent.type = SDL_EVENT_QUIT;
    SDL_PushEvent(&quitEvent);
}

} // namespace recurse::systems
