#include "recurse/systems/SaveGameSystem.hh"
#include "recurse/systems/CharacterMovementSystem.hh"
#include "recurse/systems/TerrainSystem.hh"

#include "fabric/core/AppContext.hh"
#include "fabric/core/InputRouter.hh"
#include "fabric/core/Log.hh"
#include "fabric/core/SystemRegistry.hh"
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

    auto& ecsWorld = ctx.world;
    auto& timeline = ctx.timeline;

    // F5 = quicksave
    ctx.inputRouter->registerKeyCallback(SDLK_F5, [this, &ecsWorld, &timeline]() {
        SceneSerializer qsSerializer;
        auto& pos = charMovement_->playerPosition();
        auto& vel = charMovement_->playerVelocity();
        if (saveManager_->save("quicksave", qsSerializer, ecsWorld, terrain_->density(), terrain_->essence(), timeline,
                               std::optional<fabric::Position>(fabric::Position{pos.x, pos.y, pos.z}),
                               std::optional<fabric::Position>(fabric::Position{vel.x, vel.y, vel.z}))) {
            toastManager_.show("Quick save complete", 2.0f);
            FABRIC_LOG_INFO("Quick save complete");
        } else {
            toastManager_.show("Quick save failed", 3.0f);
            FABRIC_LOG_ERROR("Quick save failed");
        }
    });

    // F9 = quickload
    ctx.inputRouter->registerKeyCallback(SDLK_F9, [this, &ecsWorld, &timeline]() {
        SceneSerializer qlSerializer;
        std::optional<fabric::Position> loadedPos;
        std::optional<fabric::Position> loadedVel;
        if (saveManager_->load("quicksave", qlSerializer, ecsWorld, terrain_->density(), terrain_->essence(), timeline,
                               loadedPos, loadedVel)) {
            if (loadedPos) {
                charMovement_->setPlayerWorldOffset(loadedPos->x, loadedPos->y, loadedPos->z);
            }
            if (loadedVel) {
                charMovement_->playerVelocity() = Velocity{loadedVel->x, loadedVel->y, loadedVel->z};
            }
            toastManager_.show("Quick load complete", 2.0f);
            FABRIC_LOG_INFO("Quick load complete");
        } else {
            toastManager_.show("Quick load failed", 3.0f);
            FABRIC_LOG_ERROR("Quick load failed");
        }
    });

    FABRIC_LOG_INFO("SaveGameSystem initialized");
}

void SaveGameSystem::fixedUpdate(fabric::AppContext& ctx, float fixedDt) {
    FABRIC_ZONE_SCOPED_N("save_game");
    auto& pos = charMovement_->playerPosition();
    auto& vel = charMovement_->playerVelocity();
    saveManager_->tickAutosave(fixedDt, saveSerializer_, ctx.world, terrain_->density(), terrain_->essence(),
                               ctx.timeline, std::optional<fabric::Position>(fabric::Position{pos.x, pos.y, pos.z}),
                               std::optional<fabric::Position>(fabric::Position{vel.x, vel.y, vel.z}));
    toastManager_.update(fixedDt);
}

void SaveGameSystem::configureDependencies() {
    after<TerrainSystem>();
    after<CharacterMovementSystem>();
}

} // namespace recurse::systems
