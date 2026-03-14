#include "recurse/systems/SaveGameSystem.hh"
#include "recurse/systems/CharacterMovementSystem.hh"
#include "recurse/systems/TerrainSystem.hh"

#include "fabric/core/AppContext.hh"
#include "fabric/core/SystemRegistry.hh"
#include "fabric/input/InputRouter.hh"
#include "fabric/log/Log.hh"
#include "fabric/utils/Profiler.hh"
#include "recurse/persistence/SaveManager.hh"

namespace recurse::systems {

SaveGameSystem::~SaveGameSystem() = default;

void SaveGameSystem::doShutdown() {
    saveManager_.reset();
    terrain_ = nullptr;
    charMovement_ = nullptr;
    FABRIC_LOG_INFO("SaveGameSystem shut down");
}

void SaveGameSystem::doInit(fabric::AppContext& ctx) {
    terrain_ = ctx.systemRegistry.get<TerrainSystem>();
    charMovement_ = ctx.systemRegistry.get<CharacterMovementSystem>();

    saveManager_ = std::make_unique<SaveManager>("saves");

    // F5 = quicksave
    ctx.inputRouter->registerKeyCallback(SDLK_F5, [this]() {
        // TODO: rewrite for SimulationGrid-based serialization
        toastManager_.show("Save disabled (pending rewrite)", 2.0f);
        FABRIC_LOG_WARN("SaveGameSystem: save disabled, pending SimulationGrid serialization rewrite");
    });

    // F9 = quickload
    ctx.inputRouter->registerKeyCallback(SDLK_F9, [this]() {
        // TODO: rewrite for SimulationGrid-based serialization
        toastManager_.show("Load disabled (pending rewrite)", 2.0f);
        FABRIC_LOG_WARN("SaveGameSystem: load disabled, pending SimulationGrid serialization rewrite");
    });

    FABRIC_LOG_INFO("SaveGameSystem initialized");
}

void SaveGameSystem::fixedUpdate(fabric::AppContext& /*ctx*/, float fixedDt) {
    FABRIC_ZONE_SCOPED_N("save_game");
    // TODO: rewrite autosave for SimulationGrid-based serialization
    toastManager_.update(fixedDt);
}

void SaveGameSystem::configureDependencies() {
    after<TerrainSystem>();
    after<CharacterMovementSystem>();
}

} // namespace recurse::systems
