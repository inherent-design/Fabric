#pragma once

namespace fabric {

// Forward declaration
class ConfigManager;

/// Called by FabricApp to enable runtime feature flag lookups.
/// If never called, FeatureFlags uses compile-time defaults.
void setFeatureFlagConfigManager(const ConfigManager* config);

/// Feature flags for incremental debugging.
/// Compile-time defaults, runtime override via ConfigManager.
///
/// Usage:
///   if (FeatureFlags::voxelRendering()) { ... }
///   if (FeatureFlags::legacyDensityField()) { ... }
///
/// Configuration via TOML:
///   [features]
///   voxel_rendering = true
///   legacy_density_field = false
///
/// Or via CLI:
///   ./recurse --feature.voxel_rendering=false
class FeatureFlags {
  public:
    // === VP0+ Voxel Pipeline ===

    /// VoxelSimulationSystem - falling sand, material simulation
    static bool voxelSimulation();

    /// VoxelMeshingSystem - SnapMC/SurfaceNets mesh generation
    static bool voxelMeshing();

    /// VoxelRenderSystem - chunk rendering
    static bool voxelRendering();

    // === Legacy Systems (to be removed) ===

    /// DensityField/EssenceField path (clean break: default false)
    static bool legacyDensityField();

    /// WaterSimulation (disabled for VP0+)
    static bool legacyWaterSim();

    /// Old TerrainGenerator
    static bool legacyTerrainGen();

    // === Rendering ===

    /// ShadowRenderSystem
    static bool shadows();

    /// OITCompositor (Order-Independent Transparency)
    static bool oit();

    /// PostProcess (bloom, tonemap)
    static bool postProcess();

    /// DebugDraw (wireframes, debug geometry)
    static bool debugDraw();

    // === Gameplay ===

    /// PhysicsWorld (Jolt integration)
    static bool physics();

    /// AudioSystem (miniaudio)
    static bool audio();

    /// ParticleSystem
    static bool particles();

    /// CharacterMovementSystem
    static bool characterMovement();

    /// BehaviorAI (BehaviorTree.CPP)
    static bool behaviorAI();

    // === UI ===

    /// RmlUi rendering
    static bool rmlUI();

    /// DebugHUD (performance overlay)
    static bool debugHUD();

    /// WAILA panel (What Am I Looking At)
    static bool waila();

    /// DevConsole
    static bool devConsole();

    // === Debug ===

    /// Tracy profiling zones
    static bool tracyProfiling();

    /// Extra debug logs (verbose)
    static bool verboseLogging();

  private:
    /// Helper: lookup key in ConfigManager, return default if not found.
    static bool get(const char* key, bool defaultVal);
};

} // namespace fabric
