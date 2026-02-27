#include "fabric/core/BehaviorAI.hh"
#include "fabric/core/ECS.hh"
#include "fabric/core/Log.hh"
#include "fabric/core/VoxelRaycast.hh"

#include <cmath>

namespace fabric {

// Action nodes

PatrolAction::PatrolAction(const std::string& name, const BT::NodeConfig& config) : BT::SyncActionNode(name, config) {}

BT::PortsList PatrolAction::providedPorts() {
    return {BT::OutputPort<int>("ai_state")};
}

BT::NodeStatus PatrolAction::tick() {
    setOutput("ai_state", static_cast<int>(AIState::Patrol));
    return BT::NodeStatus::SUCCESS;
}

ChaseAction::ChaseAction(const std::string& name, const BT::NodeConfig& config) : BT::SyncActionNode(name, config) {}

BT::PortsList ChaseAction::providedPorts() {
    return {BT::OutputPort<int>("ai_state")};
}

BT::NodeStatus ChaseAction::tick() {
    setOutput("ai_state", static_cast<int>(AIState::Chase));
    return BT::NodeStatus::SUCCESS;
}

AttackAction::AttackAction(const std::string& name, const BT::NodeConfig& config) : BT::SyncActionNode(name, config) {}

BT::PortsList AttackAction::providedPorts() {
    return {BT::OutputPort<int>("ai_state")};
}

BT::NodeStatus AttackAction::tick() {
    setOutput("ai_state", static_cast<int>(AIState::Attack));
    return BT::NodeStatus::SUCCESS;
}

FleeAction::FleeAction(const std::string& name, const BT::NodeConfig& config) : BT::SyncActionNode(name, config) {}

BT::PortsList FleeAction::providedPorts() {
    return {BT::OutputPort<int>("ai_state")};
}

BT::NodeStatus FleeAction::tick() {
    setOutput("ai_state", static_cast<int>(AIState::Flee));
    return BT::NodeStatus::SUCCESS;
}

// Condition nodes

IsPlayerNearby::IsPlayerNearby(const std::string& name, const BT::NodeConfig& config)
    : BT::ConditionNode(name, config) {}

BT::PortsList IsPlayerNearby::providedPorts() {
    return {BT::InputPort<float>("player_distance"), BT::InputPort<float>("detection_range", "10.0")};
}

BT::NodeStatus IsPlayerNearby::tick() {
    auto distance = getInput<float>("player_distance");
    auto range = getInput<float>("detection_range");
    if (!distance || !range) {
        return BT::NodeStatus::FAILURE;
    }
    return (distance.value() <= range.value()) ? BT::NodeStatus::SUCCESS : BT::NodeStatus::FAILURE;
}

IsHealthLow::IsHealthLow(const std::string& name, const BT::NodeConfig& config) : BT::ConditionNode(name, config) {}

BT::PortsList IsHealthLow::providedPorts() {
    return {BT::InputPort<float>("health"), BT::InputPort<float>("health_threshold", "30.0")};
}

BT::NodeStatus IsHealthLow::tick() {
    auto health = getInput<float>("health");
    auto threshold = getInput<float>("health_threshold");
    if (!health || !threshold) {
        return BT::NodeStatus::FAILURE;
    }
    return (health.value() <= threshold.value()) ? BT::NodeStatus::SUCCESS : BT::NodeStatus::FAILURE;
}

HasTarget::HasTarget(const std::string& name, const BT::NodeConfig& config) : BT::ConditionNode(name, config) {}

BT::PortsList HasTarget::providedPorts() {
    return {BT::InputPort<bool>("has_target")};
}

BT::NodeStatus HasTarget::tick() {
    auto target = getInput<bool>("has_target");
    if (!target) {
        return BT::NodeStatus::FAILURE;
    }
    return target.value() ? BT::NodeStatus::SUCCESS : BT::NodeStatus::FAILURE;
}

CanSeeTarget::CanSeeTarget(const std::string& name, const BT::NodeConfig& config) : BT::ConditionNode(name, config) {}

BT::PortsList CanSeeTarget::providedPorts() {
    return {BT::InputPort<float>("target_distance"), BT::InputPort<float>("target_angle"),
            BT::InputPort<float>("sight_range", "20.0"), BT::InputPort<float>("sight_angle", "120.0"),
            BT::InputPort<bool>("has_los", "true")};
}

BT::NodeStatus CanSeeTarget::tick() {
    auto dist = getInput<float>("target_distance");
    auto angle = getInput<float>("target_angle");
    auto range = getInput<float>("sight_range");
    auto fov = getInput<float>("sight_angle");
    auto los = getInput<bool>("has_los");
    if (!dist || !angle || !range || !fov || !los) {
        return BT::NodeStatus::FAILURE;
    }
    if (dist.value() > range.value())
        return BT::NodeStatus::FAILURE;
    float halfFov = fov.value() * 0.5f;
    if (angle.value() > halfFov)
        return BT::NodeStatus::FAILURE;
    if (!los.value())
        return BT::NodeStatus::FAILURE;
    return BT::NodeStatus::SUCCESS;
}

CanHearTarget::CanHearTarget(const std::string& name, const BT::NodeConfig& config) : BT::ConditionNode(name, config) {}

BT::PortsList CanHearTarget::providedPorts() {
    return {BT::InputPort<float>("target_distance"), BT::InputPort<float>("hearing_range", "10.0")};
}

BT::NodeStatus CanHearTarget::tick() {
    auto dist = getInput<float>("target_distance");
    auto range = getInput<float>("hearing_range");
    if (!dist || !range) {
        return BT::NodeStatus::FAILURE;
    }
    return (dist.value() <= range.value()) ? BT::NodeStatus::SUCCESS : BT::NodeStatus::FAILURE;
}

// BehaviorAI

void BehaviorAI::init(flecs::world& world) {
    world_ = &world;

    world.component<NPCTag>();
    world.component<AIStateComponent>();
    world.component<BehaviorTreeComponent>();
    world.component<AIAnimationMapping>();
    world.component<AIAnimationState>();
    world.component<PerceptionComponent>();

    factory_.registerNodeType<PatrolAction>("PatrolAction");
    factory_.registerNodeType<ChaseAction>("ChaseAction");
    factory_.registerNodeType<AttackAction>("AttackAction");
    factory_.registerNodeType<FleeAction>("FleeAction");
    factory_.registerNodeType<IsPlayerNearby>("IsPlayerNearby");
    factory_.registerNodeType<IsHealthLow>("IsHealthLow");
    factory_.registerNodeType<HasTarget>("HasTarget");
    factory_.registerNodeType<CanSeeTarget>("CanSeeTarget");
    factory_.registerNodeType<CanHearTarget>("CanHearTarget");

    btQuery_ = world.query_builder<BehaviorTreeComponent, AIStateComponent>().build();
    animQuery_ = world.query_builder<AIStateComponent, AIAnimationMapping, AIAnimationState>().build();

    initialized_ = true;
    FABRIC_LOG_INFO("BehaviorAI initialized (9 node types registered)");
}

void BehaviorAI::shutdown() {
    FABRIC_LOG_INFO("BehaviorAI shutting down");
    btQuery_.reset();
    animQuery_.reset();
    world_ = nullptr;
    initialized_ = false;
}

void BehaviorAI::update(float dt) {
    if (!initialized_ || !world_)
        return;

    if (!btQuery_ || !animQuery_)
        return;

    btQuery_->each([](BehaviorTreeComponent& btc, AIStateComponent& aiState) {
        if (!btc.tree.rootNode())
            return;

        auto status = btc.tree.tickOnce();

        int val;
        auto bb = btc.tree.rootBlackboard();
        if (bb && bb->get("ai_state", val)) {
            aiState.state = static_cast<AIState>(val);
        }

        if (status == BT::NodeStatus::SUCCESS || status == BT::NodeStatus::FAILURE) {
            btc.tree.haltTree();
        }
    });

    animQuery_->each(
        [dt](const AIStateComponent& aiState, const AIAnimationMapping& mapping, AIAnimationState& animState) {
            if (aiState.state != animState.previousState) {
                animState.blending = true;
                animState.blendTimer = 0.0f;
                animState.previousState = aiState.state;
            }

            if (animState.blending) {
                animState.blendTimer += dt;
                if (animState.blendTimer >= mapping.blendDuration) {
                    animState.blending = false;
                }
            }
        });
}

BT::BehaviorTreeFactory& BehaviorAI::factory() {
    return factory_;
}

BT::Tree BehaviorAI::loadBehaviorTree(const std::string& xml) {
    return factory_.createTreeFromText(xml);
}

flecs::entity BehaviorAI::createNPC(const std::string& treeXml) {
    auto entity = world_->entity().add<NPCTag>().set<AIStateComponent>({AIState::Idle});

    if (!treeXml.empty()) {
        BehaviorTreeComponent btc;
        btc.tree = factory_.createTreeFromText(treeXml);
        entity.set<BehaviorTreeComponent>(std::move(btc));
    }

    return entity;
}

void BehaviorAI::setAnimationMapping(flecs::entity npc, const AIAnimationMapping& mapping) {
    npc.set<AIAnimationMapping>(mapping);
    npc.set<AIAnimationState>({});
}

std::string BehaviorAI::getClipNameForState(const AIAnimationMapping& mapping, AIState state) {
    switch (state) {
        case AIState::Idle:
            return mapping.idleClip;
        case AIState::Patrol:
            return mapping.patrolClip;
        case AIState::Chase:
            return mapping.chaseClip;
        case AIState::Attack:
            return mapping.attackClip;
        case AIState::Flee:
            return mapping.fleeClip;
    }
    return mapping.idleClip;
}

void BehaviorAI::setPerceptionConfig(flecs::entity npc, const PerceptionConfig& config) {
    npc.set<PerceptionComponent>({config});
}

std::vector<Vec3f> BehaviorAI::getEntitiesInRange(const Vec3f& pos, float range) {
    std::vector<Vec3f> results;
    if (!initialized_ || !world_)
        return results;

    float rangeSq = range * range;
    auto q = world_->query_builder<const NPCTag, const AIStateComponent>().with<Position>().build();
    q.each([&](flecs::entity e, const NPCTag&, const AIStateComponent&) {
        if (!e.has<Position>())
            return;
        const auto& p = e.get<Position>();
        Vec3f epos(p.x, p.y, p.z);
        Vec3f diff = epos - pos;
        if (diff.lengthSquared() <= rangeSq) {
            results.push_back(epos);
        }
    });
    return results;
}

bool BehaviorAI::hasLineOfSight(const ChunkedGrid<float>& grid, const Vec3f& from, const Vec3f& to) {
    Vec3f dir = to - from;
    float dist = dir.length();
    if (dist < 1e-6f)
        return true;

    Vec3f d = dir / dist;
    auto hit = castRay(grid, from.x, from.y, from.z, d.x, d.y, d.z, dist);
    return !hit.has_value();
}

} // namespace fabric
