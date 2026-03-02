#pragma once

#include "fabric/core/SystemBase.hh"
#include "fabric/ui/DebugHUD.hh"
#include "recurse/ai/BTDebugPanel.hh"
#include "recurse/render/DebugDraw.hh"
#include "recurse/ui/ContentBrowser.hh"
#include "recurse/ui/DevConsole.hh"

#include <flecs.h>

namespace recurse::systems {

class CameraGameSystem;
class ChunkPipelineSystem;
class PhysicsGameSystem;
class AudioGameSystem;
class AIGameSystem;
class TerrainSystem;
class CharacterMovementSystem;
class VoxelRenderSystem;
class OITRenderSystem;

/// Debug overlays, HUD, panels, and developer console.
/// Registered to the Render phase (after VoxelRenderSystem and OITRenderSystem)
/// so that debugDraw.begin()/end() operates on the geometry view before bgfx
/// frame submission. RmlUi panels update independently.
class DebugOverlaySystem : public fabric::System<DebugOverlaySystem> {
  public:
    void init(fabric::AppContext& ctx) override;
    void render(fabric::AppContext& ctx) override;
    void shutdown() override;
    void configureDependencies() override;

    recurse::DebugDraw& debugDraw() { return debugDraw_; }
    fabric::DebugHUD& debugHUD() { return debugHUD_; }
    recurse::BTDebugPanel& btDebugPanel() { return btDebugPanel_; }
    recurse::ContentBrowser& contentBrowser() { return contentBrowser_; }
    recurse::DevConsole& devConsole() { return devConsole_; }

  private:
    recurse::DebugDraw debugDraw_;
    fabric::DebugHUD debugHUD_;
    recurse::BTDebugPanel btDebugPanel_;
    recurse::ContentBrowser contentBrowser_;
    recurse::DevConsole devConsole_;

    flecs::entity btDebugSelectedNpc_;
    int frameCounter_ = 0;

    CameraGameSystem* camera_ = nullptr;
    ChunkPipelineSystem* chunks_ = nullptr;
    PhysicsGameSystem* physics_ = nullptr;
    AudioGameSystem* audio_ = nullptr;
    AIGameSystem* ai_ = nullptr;
    TerrainSystem* terrain_ = nullptr;
    CharacterMovementSystem* charMovement_ = nullptr;
};

} // namespace recurse::systems
