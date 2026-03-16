#pragma once

namespace fabric {
class ConfigManager;
} // namespace fabric

namespace recurse {

/// Runtime-tunable configuration for Recurse game systems.
/// Loaded from recurse.toml via ConfigManager. Constexpr defaults
/// serve as fallbacks when TOML keys are missing.
struct RecurseConfig {

    // -- Pipeline budgets --
    static constexpr int K_DEFAULT_MAX_ASYNC_LOADS = 32;
    static constexpr int K_DEFAULT_MAX_GENERATES = 512;
    static constexpr int K_DEFAULT_MAX_LOAD_COMPLETIONS = 16;
    static constexpr int K_DEFAULT_LOD_HYSTERESIS = 2;

    int maxAsyncLoads = K_DEFAULT_MAX_ASYNC_LOADS;
    int maxGenerates = K_DEFAULT_MAX_GENERATES;
    int maxLoadCompletions = K_DEFAULT_MAX_LOAD_COMPLETIONS;
    int lodHysteresis = K_DEFAULT_LOD_HYSTERESIS;

    // -- Streaming AIMD --
    static constexpr float K_DEFAULT_TARGET_HIGH_MS = 16.0f;
    static constexpr float K_DEFAULT_TARGET_LOW_MS = 10.0f;
    static constexpr int K_DEFAULT_STREAMING_FLOOR = 4;

    float targetHighMs = K_DEFAULT_TARGET_HIGH_MS;
    float targetLowMs = K_DEFAULT_TARGET_LOW_MS;
    int streamingFloor = K_DEFAULT_STREAMING_FLOOR;

    // -- Physics --
    static constexpr int K_DEFAULT_COLLISION_BUDGET = 8;
    static constexpr int K_DEFAULT_COLLISION_RADIUS = 3;

    int collisionBudget = K_DEFAULT_COLLISION_BUDGET;
    int collisionRadius = K_DEFAULT_COLLISION_RADIUS;

    // -- Audio --
    static constexpr int K_DEFAULT_MAX_OCCLUSION_VOXELS = 8;

    int maxOcclusionVoxels = K_DEFAULT_MAX_OCCLUSION_VOXELS;

    // -- Simulation --
    static constexpr int K_DEFAULT_MAX_FIXED_STEPS = 3;

    int maxFixedSteps = K_DEFAULT_MAX_FIXED_STEPS;

    /// Load from ConfigManager, using constexpr defaults as fallbacks.
    static RecurseConfig loadFromConfig(const fabric::ConfigManager& config);
};

} // namespace recurse
