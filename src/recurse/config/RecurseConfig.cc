#include "recurse/config/RecurseConfig.hh"
#include "fabric/platform/ConfigManager.hh"

namespace recurse {

RecurseConfig RecurseConfig::loadFromConfig(const fabric::ConfigManager& config) {
    RecurseConfig c;

    c.maxAsyncLoads = config.get<int>("pipeline.max_async_loads", K_DEFAULT_MAX_ASYNC_LOADS);
    c.maxGenerates = config.get<int>("pipeline.max_generates", K_DEFAULT_MAX_GENERATES);
    c.maxLoadCompletions = config.get<int>("pipeline.max_load_completions", K_DEFAULT_MAX_LOAD_COMPLETIONS);
    c.lodHysteresis = config.get<int>("pipeline.lod_hysteresis", K_DEFAULT_LOD_HYSTERESIS);

    c.targetHighMs = config.get<float>("streaming.target_high_ms", K_DEFAULT_TARGET_HIGH_MS);
    c.targetLowMs = config.get<float>("streaming.target_low_ms", K_DEFAULT_TARGET_LOW_MS);
    c.streamingFloor = config.get<int>("streaming.floor", K_DEFAULT_STREAMING_FLOOR);

    c.collisionBudget = config.get<int>("physics.collision_budget", K_DEFAULT_COLLISION_BUDGET);
    c.collisionRadius = config.get<int>("physics.collision_radius", K_DEFAULT_COLLISION_RADIUS);

    c.maxOcclusionVoxels = config.get<int>("audio.max_occlusion_voxels", K_DEFAULT_MAX_OCCLUSION_VOXELS);

    c.maxFixedSteps = config.get<int>("simulation.max_fixed_steps", K_DEFAULT_MAX_FIXED_STEPS);

    return c;
}

} // namespace recurse
