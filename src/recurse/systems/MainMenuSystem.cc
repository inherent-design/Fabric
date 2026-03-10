#include "recurse/systems/MainMenuSystem.hh"
#include "recurse/character/GameConstants.hh"
#include "recurse/persistence/WorldRegistry.hh"

#include "fabric/core/AppContext.hh"
#include "fabric/core/AppModeManager.hh"
#include "fabric/core/Event.hh"
#include "fabric/core/Log.hh"
#include "fabric/core/SystemRegistry.hh"
#include "recurse/systems/AIGameSystem.hh"
#include "recurse/systems/AudioGameSystem.hh"
#include "recurse/systems/CameraGameSystem.hh"
#include "recurse/systems/CharacterMovementSystem.hh"
#include "recurse/systems/ChunkPipelineSystem.hh"
#include "recurse/systems/LODSystem.hh"
#include "recurse/systems/OITRenderSystem.hh"
#include "recurse/systems/ParticleGameSystem.hh"
#include "recurse/systems/PhysicsGameSystem.hh"
#include "recurse/systems/SaveGameSystem.hh"
#include "recurse/systems/ShadowRenderSystem.hh"
#include "recurse/systems/TerrainSystem.hh"
#include "recurse/systems/VoxelInteractionSystem.hh"
#include "recurse/systems/VoxelMeshingSystem.hh"
#include "recurse/systems/VoxelRenderSystem.hh"
#include "recurse/systems/VoxelSimulationSystem.hh"
#include "recurse/world/NaturalWorldGenerator.hh"
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
    registry_ = &ctx.systemRegistry;
    rmlContext_ = ctx.rmlContext;

    FABRIC_LOG_INFO("MainMenuSystem: rmlContext={}, appModeManager={}", (void*)rmlContext_, (void*)appModeManager_);

    if (!rmlContext_) {
        FABRIC_LOG_ERROR("MainMenuSystem: No RmlUi context available");
        return;
    }

    initRmlDataModel();

    // Set up start game callback
    startGameCallback_ = [this](WorldType type, int64_t seed) {
        if (!terrain_) {
            FABRIC_LOG_ERROR("MainMenuSystem: Cannot start game - no TerrainSystem");
            return;
        }

        // Set world generator based on selection, using the provided seed
        if (type == WorldType::Natural) {
            auto config = fabric::world::NoiseGenConfig{};
            config.seed = static_cast<int>(seed);
            terrain_->setWorldGenerator(std::make_unique<recurse::NaturalWorldGenerator>(config));
            FABRIC_LOG_INFO("MainMenu: Using NaturalWorldGenerator (seed={})", seed);
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

        // World is ready; enable world-dependent systems before entering Game mode.
        setWorldSystemsEnabled(true);

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

    // No world exists at startup; disable world-dependent systems until world creation.
    setWorldSystemsEnabled(false);

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
    registry_ = nullptr;
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
        case MenuState::WorldCreate:
            showWorldCreate();
            break;
        case MenuState::WorldList:
            showWorldList();
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

void MainMenuSystem::showWorldCreate() {
    showDocument("assets/ui/new_world.rml");

    // Reset input state
    newWorldName_ = "New World";
    newWorldType_ = "Natural";
    newWorldSeed_.clear();

    if (currentDocument_) {
        auto* typeFlat = currentDocument_->GetElementById("btn_type_flat");
        if (typeFlat) {
            typeFlat->AddEventListener(Rml::EventId::Click,
                                       new MenuButtonListener([this]() { newWorldType_ = "Flat"; }), true);
        }

        auto* typeNatural = currentDocument_->GetElementById("btn_type_natural");
        if (typeNatural) {
            typeNatural->AddEventListener(Rml::EventId::Click,
                                          new MenuButtonListener([this]() { newWorldType_ = "Natural"; }), true);
        }

        auto* createBtn = currentDocument_->GetElementById("btn_create");
        if (createBtn) {
            createBtn->AddEventListener(Rml::EventId::Click, new MenuButtonListener([this]() {
                                            // Read input values from RML elements before creating
                                            if (currentDocument_) {
                                                auto* nameInput = currentDocument_->GetElementById("input_name");
                                                if (nameInput) {
                                                    newWorldName_ =
                                                        nameInput->GetAttribute("value", Rml::String("New World"));
                                                }
                                                auto* seedInput = currentDocument_->GetElementById("input_seed");
                                                if (seedInput) {
                                                    newWorldSeed_ = seedInput->GetAttribute("value", Rml::String(""));
                                                }
                                            }
                                            onCreateWorldClicked();
                                        }),
                                        true);
        }

        auto* backBtn = currentDocument_->GetElementById("btn_back");
        if (backBtn) {
            backBtn->AddEventListener(Rml::EventId::Click, new MenuButtonListener([this]() { onBackClicked(); }), true);
        }
    }
}

void MainMenuSystem::showWorldList() {
    showDocument("assets/ui/world_list.rml");

    if (currentDocument_) {
        // Populate world list dynamically
        if (worldRegistry_) {
            auto worlds = worldRegistry_->listWorlds();
            auto* container = currentDocument_->GetElementById("worlds_container");

            if (container) {
                // Clear existing children
                while (container->GetNumChildren() > 0) {
                    container->RemoveChild(container->GetFirstChild());
                }

                for (const auto& world : worlds) {
                    // Create a button element for each world
                    auto el = container->AppendChild(currentDocument_->CreateElement("button"));
                    el->SetClassNames("menu_button wide");
                    el->SetId("world_" + world.uuid);

                    std::string typeStr = (world.type == WorldType::Flat) ? "Flat" : "Natural";
                    el->SetInnerRML("<span class='btn_title'>" + world.name +
                                    "</span>"
                                    "<span class='btn_desc'>" +
                                    typeStr + " | " + world.lastPlayed + "</span>");

                    std::string uuid = world.uuid;
                    el->AddEventListener(Rml::EventId::Click,
                                         new MenuButtonListener([this, uuid]() { onWorldSelected(uuid); }), true);
                }
            }
        }

        auto* newWorldBtn = currentDocument_->GetElementById("btn_new_world");
        if (newWorldBtn) {
            newWorldBtn->AddEventListener(Rml::EventId::Click,
                                          new MenuButtonListener([this]() { onNewWorldClicked(); }), true);
        }

        auto* backBtn = currentDocument_->GetElementById("btn_back");
        if (backBtn) {
            backBtn->AddEventListener(Rml::EventId::Click,
                                      new MenuButtonListener([this]() { transitionTo(MenuState::TitleScreen); }), true);
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
    // If WorldRegistry is available, show world list; otherwise fall back to quick select
    if (worldRegistry_) {
        auto worlds = worldRegistry_->listWorlds();
        if (worlds.empty()) {
            transitionTo(MenuState::WorldCreate);
        } else {
            transitionTo(MenuState::WorldList);
        }
    } else {
        transitionTo(MenuState::WorldSelect);
    }
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
        auto seed = static_cast<int64_t>(std::chrono::steady_clock::now().time_since_epoch().count());
        startGameCallback_(WorldType::Flat, seed);
    }
    transitionTo(MenuState::Hidden);
}

void MainMenuSystem::onMinecraftWorldClicked() {
    FABRIC_LOG_INFO("MainMenu: Minecraft world selected");
    if (startGameCallback_) {
        auto seed = static_cast<int64_t>(std::chrono::steady_clock::now().time_since_epoch().count());
        startGameCallback_(WorldType::Natural, seed);
    }
    transitionTo(MenuState::Hidden);
}

void MainMenuSystem::onBackClicked() {
    FABRIC_LOG_DEBUG("MainMenu: Back clicked");
    // Context-dependent back: WorldCreate -> WorldList (if registry), else TitleScreen
    if (menuState_ == MenuState::WorldCreate && worldRegistry_) {
        auto worlds = worldRegistry_->listWorlds();
        if (!worlds.empty()) {
            transitionTo(MenuState::WorldList);
            return;
        }
    }
    transitionTo(MenuState::TitleScreen);
}

void MainMenuSystem::onNewWorldClicked() {
    FABRIC_LOG_INFO("MainMenu: New World clicked");
    transitionTo(MenuState::WorldCreate);
}

void MainMenuSystem::onLoadWorldClicked() {
    FABRIC_LOG_INFO("MainMenu: Load World clicked");
    transitionTo(MenuState::WorldList);
}

void MainMenuSystem::onCreateWorldClicked() {
    if (!worldRegistry_) {
        FABRIC_LOG_ERROR("MainMenu: No WorldRegistry, cannot create world");
        return;
    }

    // Read inputs from the RML document
    std::string worldName = newWorldName_;
    if (worldName.empty())
        worldName = "New World";

    WorldType type = (newWorldType_ == "Flat") ? WorldType::Flat : WorldType::Natural;

    int64_t seed = 0;
    if (!newWorldSeed_.empty()) {
        try {
            seed = std::stoll(newWorldSeed_);
        } catch (...) {
            // Use hash of string as seed
            seed = static_cast<int64_t>(std::hash<std::string>{}(newWorldSeed_));
        }
    } else {
        seed = static_cast<int64_t>(std::chrono::steady_clock::now().time_since_epoch().count());
    }

    auto meta = worldRegistry_->createWorld(worldName, type, seed);
    FABRIC_LOG_INFO("MainMenu: Created world '{}' (uuid={}, type={}, seed={})", meta.name, meta.uuid,
                    static_cast<int>(meta.type), meta.seed);

    // Start the game with this world type and seed
    if (startGameCallback_) {
        startGameCallback_(type, meta.seed);
    }
    transitionTo(MenuState::Hidden);
}

void MainMenuSystem::onDeleteWorldClicked(const std::string& uuid) {
    if (!worldRegistry_)
        return;

    bool deleted = worldRegistry_->deleteWorld(uuid);
    FABRIC_LOG_INFO("MainMenu: Delete world {} -> {}", uuid, deleted ? "success" : "failed");

    // Refresh the list
    if (menuState_ == MenuState::WorldList) {
        showWorldList();
    }
}

void MainMenuSystem::onRenameWorldClicked(const std::string& uuid, const std::string& newName) {
    if (!worldRegistry_)
        return;

    bool renamed = worldRegistry_->renameWorld(uuid, newName);
    FABRIC_LOG_INFO("MainMenu: Rename world {} to '{}' -> {}", uuid, newName, renamed ? "success" : "failed");

    if (menuState_ == MenuState::WorldList) {
        showWorldList();
    }
}

void MainMenuSystem::onOpenFolderClicked(const std::string& uuid) {
    if (!worldRegistry_)
        return;

    std::string path = worldRegistry_->worldPath(uuid);
    FABRIC_LOG_INFO("MainMenu: Open folder {}", path);

    std::string url = "file://" + path;
    if (!SDL_OpenURL(url.c_str())) {
        FABRIC_LOG_ERROR("MainMenu: SDL_OpenURL failed for {}: {}", url, SDL_GetError());
    }
}

void MainMenuSystem::onWorldSelected(const std::string& uuid) {
    if (!worldRegistry_)
        return;

    auto meta = worldRegistry_->getWorld(uuid);
    if (!meta) {
        FABRIC_LOG_ERROR("MainMenu: World {} not found", uuid);
        return;
    }

    worldRegistry_->touchWorld(uuid);
    FABRIC_LOG_INFO("MainMenu: Loading world '{}' (type={}, seed={})", meta->name, static_cast<int>(meta->type),
                    meta->seed);

    if (startGameCallback_) {
        startGameCallback_(meta->type, meta->seed);
    }
    transitionTo(MenuState::Hidden);
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

    setWorldSystemsEnabled(false);
}

void MainMenuSystem::onExitToDesktopClicked() {
    FABRIC_LOG_INFO("MainMenu: Exit to Desktop clicked");
    // Signal app to quit
    SDL_Event quitEvent;
    quitEvent.type = SDL_EVENT_QUIT;
    SDL_PushEvent(&quitEvent);
}

void MainMenuSystem::setWorldSystemsEnabled(bool enabled) {
    if (!registry_)
        return;

    // 16 world-dependent systems. MainMenuSystem and DebugOverlaySystem stay always-enabled.
    registry_->setEnabled<TerrainSystem>(enabled);
    registry_->setEnabled<VoxelSimulationSystem>(enabled);
    registry_->setEnabled<PhysicsGameSystem>(enabled);
    registry_->setEnabled<CharacterMovementSystem>(enabled);
    registry_->setEnabled<AIGameSystem>(enabled);
    registry_->setEnabled<ParticleGameSystem>(enabled);
    registry_->setEnabled<ChunkPipelineSystem>(enabled);
    registry_->setEnabled<VoxelInteractionSystem>(enabled);
    registry_->setEnabled<SaveGameSystem>(enabled);
    registry_->setEnabled<AudioGameSystem>(enabled);
    registry_->setEnabled<CameraGameSystem>(enabled);
    registry_->setEnabled<VoxelMeshingSystem>(enabled);
    registry_->setEnabled<LODSystem>(enabled);
    registry_->setEnabled<ShadowRenderSystem>(enabled);
    registry_->setEnabled<VoxelRenderSystem>(enabled);
    registry_->setEnabled<OITRenderSystem>(enabled);

    FABRIC_LOG_INFO("World systems {}", enabled ? "enabled" : "disabled");
}

} // namespace recurse::systems
