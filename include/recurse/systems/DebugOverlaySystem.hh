#pragma once

#include "fabric/core/SystemBase.hh"
#include "fabric/ui/ChunkDebugPanel.hh"
#include "fabric/ui/DebugHUD.hh"
#include "fabric/ui/HotkeyPanel.hh"
#include "fabric/ui/WAILAPanel.hh"
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
class VoxelMeshingSystem;
class VoxelSimulationSystem;

/// Debug overlays, HUD, panels, and developer console.
/// Registered to the Render phase (after VoxelRenderSystem and OITRenderSystem)
/// so that debugDraw.begin()/end() operates on the geometry view before bgfx
/// frame submission. RmlUi panels update independently.
class DebugOverlaySystem : public fabric::System<DebugOverlaySystem> {
  public:
    void doInit(fabric::AppContext& ctx) override;
    void render(fabric::AppContext& ctx) override;
    void doShutdown() override;
    void configureDependencies() override;

    recurse::DebugDraw& debugDraw() { return debugDraw_; }
    fabric::DebugHUD& debugHUD() { return debugHUD_; }
    fabric::ChunkDebugPanel& chunkDebugPanel() { return chunkDebugPanel_; }
    recurse::BTDebugPanel& btDebugPanel() { return btDebugPanel_; }
    recurse::ContentBrowser& contentBrowser() { return contentBrowser_; }
    recurse::DevConsole& devConsole() { return devConsole_; }
    fabric::WAILAPanel& wailaPanel() { return wailaPanel_; }
    fabric::HotkeyPanel& hotkeyPanel() { return hotkeyPanel_; }

  private:
    recurse::DebugDraw debugDraw_;
    fabric::DebugHUD debugHUD_;
    fabric::ChunkDebugPanel chunkDebugPanel_;
    fabric::WAILAPanel wailaPanel_;
    fabric::HotkeyPanel hotkeyPanel_;
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
    VoxelMeshingSystem* meshSystem_ = nullptr;
    VoxelSimulationSystem* voxelSim_ = nullptr;
};

} // namespace recurse::systems
