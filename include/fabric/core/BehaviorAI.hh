#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <behaviortree_cpp/bt_factory.h>
#include <flecs.h>

#include "fabric/core/Animation.hh"
#include "fabric/core/ChunkedGrid.hh"
#include "fabric/core/Rendering.hh"
#include "fabric/utils/BVH.hh"

namespace fabric {

// AI behavioral state for NPC decision-making
enum class AIState : uint8_t {
    Idle,
    Patrol,
    Chase,
    Attack,
    Flee
};

// ECS components

struct NPCTag {};

struct AIStateComponent {
    AIState state = AIState::Idle;
};

struct BehaviorTreeComponent {
    BT::Tree tree;
};

struct AIAnimationMapping {
    std::string idleClip = "idle";
    std::string patrolClip = "walk";
    std::string chaseClip = "run";
    std::string attackClip = "attack";
    std::string fleeClip = "run_fast";
    float blendDuration = 0.2f;
};

struct AIAnimationState {
    AIState previousState = AIState::Idle;
    float blendTimer = 0.0f;
    bool blending = false;
};

struct PerceptionConfig {
    float sightRange = 20.0f;
    float hearingRange = 10.0f;
    float sightAngle = 120.0f; // degrees, forward cone
};

struct PerceptionComponent {
    PerceptionConfig config;
    Vec3f forward = {0.0f, 0.0f, 1.0f};
};

// BT action nodes: set AIState via output port, return SUCCESS

class PatrolAction : public BT::SyncActionNode {
  public:
    PatrolAction(const std::string& name, const BT::NodeConfig& config);
    static BT::PortsList providedPorts();
    BT::NodeStatus tick() override;
};

class ChaseAction : public BT::SyncActionNode {
  public:
    ChaseAction(const std::string& name, const BT::NodeConfig& config);
    static BT::PortsList providedPorts();
    BT::NodeStatus tick() override;
};

class AttackAction : public BT::SyncActionNode {
  public:
    AttackAction(const std::string& name, const BT::NodeConfig& config);
    static BT::PortsList providedPorts();
    BT::NodeStatus tick() override;
};

class FleeAction : public BT::SyncActionNode {
  public:
    FleeAction(const std::string& name, const BT::NodeConfig& config);
    static BT::PortsList providedPorts();
    BT::NodeStatus tick() override;
};

// BT condition nodes: read blackboard inputs, return SUCCESS or FAILURE

class IsPlayerNearby : public BT::ConditionNode {
  public:
    IsPlayerNearby(const std::string& name, const BT::NodeConfig& config);
    static BT::PortsList providedPorts();
    BT::NodeStatus tick() override;
};

class IsHealthLow : public BT::ConditionNode {
  public:
    IsHealthLow(const std::string& name, const BT::NodeConfig& config);
    static BT::PortsList providedPorts();
    BT::NodeStatus tick() override;
};

class HasTarget : public BT::ConditionNode {
  public:
    HasTarget(const std::string& name, const BT::NodeConfig& config);
    static BT::PortsList providedPorts();
    BT::NodeStatus tick() override;
};

class CanSeeTarget : public BT::ConditionNode {
  public:
    CanSeeTarget(const std::string& name, const BT::NodeConfig& config);
    static BT::PortsList providedPorts();
    BT::NodeStatus tick() override;
};

class CanHearTarget : public BT::ConditionNode {
  public:
    CanHearTarget(const std::string& name, const BT::NodeConfig& config);
    static BT::PortsList providedPorts();
    BT::NodeStatus tick() override;
};

// Manages NPC entities with behavior trees for decision-making.
// Wraps BehaviorTree.CPP factory and provides Flecs ECS integration.
class BehaviorAI {
  public:
    void init(flecs::world& world);
    void shutdown();
    void update(float dt);

    BT::BehaviorTreeFactory& factory();
    BT::Tree loadBehaviorTree(const std::string& xml);
    flecs::entity createNPC(const std::string& treeXml);

    void setAnimationMapping(flecs::entity npc, const AIAnimationMapping& mapping);
    std::string getClipNameForState(const AIAnimationMapping& mapping, AIState state);

    void setPerceptionConfig(flecs::entity npc, const PerceptionConfig& config);
    std::vector<Vec3f> getEntitiesInRange(const Vec3f& pos, float range);
    static bool hasLineOfSight(const ChunkedGrid<float>& grid, const Vec3f& from, const Vec3f& to);

  private:
    BT::BehaviorTreeFactory factory_;
    flecs::world* world_ = nullptr;
    bool initialized_ = false;

    // Cached queries (built once in init, avoid per-frame rebuild).
    // Wrapped in optional to safely reset without touching dead world.
    std::optional<flecs::query<BehaviorTreeComponent, AIStateComponent>> btQuery_;
    std::optional<flecs::query<AIStateComponent, AIAnimationMapping, AIAnimationState>> animQuery_;

    // Spatial index for O(log n) perception queries.
    // Rebuilt per-update to track entity position changes.
    BVH<flecs::entity> spatialIndex_;

    void rebuildSpatialIndex();
};

} // namespace fabric
