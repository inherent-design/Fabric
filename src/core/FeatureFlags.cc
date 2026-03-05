#include "fabric/core/FeatureFlags.hh"

#include "fabric/core/AppContext.hh"
#include "fabric/core/ConfigManager.hh"
#include "fabric/core/Log.hh"

namespace fabric {

namespace {
/// Global ConfigManager pointer for FeatureFlags lookups.
/// Set by FabricApp during initialization. Safe to be null (uses defaults).
const ConfigManager* g_configManager = nullptr;
} // namespace

/// Called by FabricApp to enable runtime feature flag lookups.
/// If never called, FeatureFlags uses compile-time defaults.
void setFeatureFlagConfigManager(const ConfigManager* config) {
    g_configManager = config;
    if (config) {
        FABRIC_LOG_DEBUG("FeatureFlags: ConfigManager bound for runtime overrides");
    }
}

// -- Private helper --

bool FeatureFlags::get(const char* key, bool defaultVal) {
    if (g_configManager) {
        return g_configManager->get<bool>(key, defaultVal);
    }
    // ConfigManager not ready - use compile-time default
    return defaultVal;
}

// -- VP0+ Voxel Pipeline --

bool FeatureFlags::voxelSimulation() {
    return get("features.voxel_simulation", true);
}

bool FeatureFlags::voxelMeshing() {
    return get("features.voxel_meshing", true);
}

bool FeatureFlags::voxelRendering() {
    return get("features.voxel_rendering", true);
}

// -- Legacy Systems (clean break: all default false) --

bool FeatureFlags::legacyDensityField() {
    return get("features.legacy_density_field", false);
}

bool FeatureFlags::legacyWaterSim() {
    return get("features.legacy_water_sim", false);
}

bool FeatureFlags::legacyTerrainGen() {
    return get("features.legacy_terrain_gen", false);
}

// -- Rendering --

bool FeatureFlags::shadows() {
    return get("features.shadows", true);
}

bool FeatureFlags::oit() {
    return get("features.oit", false);
}

bool FeatureFlags::postProcess() {
    return get("features.post_process", true);
}

bool FeatureFlags::debugDraw() {
    return get("features.debug_draw", true);
}

// -- Gameplay --

bool FeatureFlags::physics() {
    return get("features.physics", true);
}

bool FeatureFlags::audio() {
    return get("features.audio", false);
}

bool FeatureFlags::particles() {
    return get("features.particles", false);
}

bool FeatureFlags::characterMovement() {
    return get("features.character_movement", true);
}

bool FeatureFlags::behaviorAI() {
    return get("features.behavior_ai", false);
}

// -- UI --

bool FeatureFlags::rmlUI() {
    return get("features.rml_ui", true);
}

bool FeatureFlags::debugHUD() {
    return get("features.debug_hud", true);
}

bool FeatureFlags::waila() {
    return get("features.waila", true);
}

bool FeatureFlags::devConsole() {
    return get("features.dev_console", true);
}

// -- Debug --

bool FeatureFlags::tracyProfiling() {
    return get("features.tracy_profiling", true);
}

bool FeatureFlags::verboseLogging() {
    return get("features.verbose_logging", false);
}

} // namespace fabric
