#pragma once

#include "fabric/core/SystemBase.hh"
#include "fabric/ui/ConcurrencyPanel.hh"
#include "fabric/ui/HotkeyPanel.hh"
#include "recurse/ai/BTDebugPanel.hh"
#include "recurse/render/DebugDraw.hh"
#include "recurse/ui/ChunkDebugPanel.hh"
#include "recurse/ui/ContentBrowser.hh"
#include "recurse/ui/DebugHUD.hh"
#include "recurse/ui/DevConsole.hh"
#include "recurse/ui/LODStatsPanel.hh"
#include "recurse/ui/WAILAPanel.hh"

#include <flecs.h>

namespace recurse::systems {

class CameraGameSystem;
class ChunkPipelineSystem;
class LODSystem;
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

    void onWorldBegin();
    void onWorldEnd();

    recurse::DebugDraw& debugDraw() { return debugDraw_; }
    recurse::DebugHUD& debugHUD() { return debugHUD_; }
    recurse::ChunkDebugPanel& chunkDebugPanel() { return chunkDebugPanel_; }
    recurse::LODStatsPanel& lodStatsPanel() { return lodStatsPanel_; }
    fabric::ConcurrencyPanel& concurrencyPanel() { return concurrencyPanel_; }
    recurse::BTDebugPanel& btDebugPanel() { return btDebugPanel_; }
    recurse::ContentBrowser& contentBrowser() { return contentBrowser_; }
    recurse::DevConsole& devConsole() { return devConsole_; }
    recurse::WAILAPanel& wailaPanel() { return wailaPanel_; }
    fabric::HotkeyPanel& hotkeyPanel() { return hotkeyPanel_; }

  private:
    recurse::DebugDraw debugDraw_;
    recurse::DebugHUD debugHUD_;
    recurse::ChunkDebugPanel chunkDebugPanel_;
    recurse::LODStatsPanel lodStatsPanel_;
    fabric::ConcurrencyPanel concurrencyPanel_;
    recurse::WAILAPanel wailaPanel_;
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
    LODSystem* lodSystem_ = nullptr;
    VoxelMeshingSystem* meshSystem_ = nullptr;
    VoxelSimulationSystem* voxelSim_ = nullptr;
};

} // namespace recurse::systems
