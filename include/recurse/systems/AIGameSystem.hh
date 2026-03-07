#pragma once

#include "fabric/core/SystemBase.hh"
#include "recurse/ai/BehaviorAI.hh"
#include "recurse/ai/Pathfinding.hh"
#include "recurse/animation/AnimationEvents.hh"

namespace recurse::systems {

/// Owns behavior tree AI, pathfinding, and animation event subsystems.
/// No cross-system dependencies; updates behavior trees at fixed rate.
class AIGameSystem : public fabric::System<AIGameSystem> {
  public:
    AIGameSystem() = default;

    void doInit(fabric::AppContext& ctx) override;
    void doShutdown() override;
    void fixedUpdate(fabric::AppContext& ctx, float fixedDt) override;

    void configureDependencies() override;

    BehaviorAI& behaviorAI() { return behaviorAI_; }
    const BehaviorAI& behaviorAI() const { return behaviorAI_; }
    Pathfinding& pathfinding() { return pathfinding_; }
    AnimationEvents& animEvents() { return animEvents_; }

  private:
    BehaviorAI behaviorAI_;
    Pathfinding pathfinding_;
    AnimationEvents animEvents_;
};

} // namespace recurse::systems
