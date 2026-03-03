#include "recurse/systems/AIGameSystem.hh"

#include "fabric/core/AppContext.hh"
#include "fabric/core/ECS.hh"
#include "fabric/core/Log.hh"
#include "fabric/utils/Profiler.hh"

namespace recurse::systems {

void AIGameSystem::init(fabric::AppContext& ctx) {
    behaviorAI_.init(ctx.world.get());
    pathfinding_.init();
    animEvents_.init();

    FABRIC_LOG_INFO("AIGameSystem initialized");
}

void AIGameSystem::shutdown() {
    animEvents_.shutdown();
    pathfinding_.shutdown();
    behaviorAI_.shutdown();
}

void AIGameSystem::fixedUpdate(fabric::AppContext& /*ctx*/, float fixedDt) {
    FABRIC_ZONE_SCOPED_N("ai_update");
    behaviorAI_.update(fixedDt);
}

void AIGameSystem::configureDependencies() {
    // No dependencies; AI runs independently
}

} // namespace recurse::systems
