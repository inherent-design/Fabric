#pragma once

#include "fabric/core/SystemBase.hh"
#include "fabric/ui/ToastManager.hh"
#include "recurse/persistence/SceneSerializer.hh"
#include <memory>

namespace recurse {
class SaveManager;
} // namespace recurse

namespace recurse::systems {

class TerrainSystem;
class CharacterMovementSystem;

/// Owns save/load orchestration, autosave timer, and toast notifications.
/// Registers F5 (quicksave) and F9 (quickload) key callbacks during init.
class SaveGameSystem : public fabric::System<SaveGameSystem> {
  public:
    SaveGameSystem() = default;

    void init(fabric::AppContext& ctx) override;
    void fixedUpdate(fabric::AppContext& ctx, float fixedDt) override;

    void configureDependencies() override;

    fabric::ToastManager& toastManager() { return toastManager_; }
    const fabric::ToastManager& toastManager() const { return toastManager_; }

  private:
    TerrainSystem* terrain_ = nullptr;
    CharacterMovementSystem* charMovement_ = nullptr;

    std::unique_ptr<SaveManager> saveManager_;
    SceneSerializer saveSerializer_;
    fabric::ToastManager toastManager_;
};

} // namespace recurse::systems
