#include "fabric/core/BehaviorAI.hh"
#include "fabric/core/Log.hh"

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

// BehaviorAI

void BehaviorAI::init(flecs::world& world) {
    world_ = &world;

    world.component<NPCTag>();
    world.component<AIStateComponent>();
    world.component<BehaviorTreeComponent>();
    world.component<AIAnimationMapping>();
    world.component<AIAnimationState>();

    factory_.registerNodeType<PatrolAction>("PatrolAction");
    factory_.registerNodeType<ChaseAction>("ChaseAction");
    factory_.registerNodeType<AttackAction>("AttackAction");
    factory_.registerNodeType<FleeAction>("FleeAction");
    factory_.registerNodeType<IsPlayerNearby>("IsPlayerNearby");
    factory_.registerNodeType<IsHealthLow>("IsHealthLow");
    factory_.registerNodeType<HasTarget>("HasTarget");

    initialized_ = true;
    FABRIC_LOG_INFO("BehaviorAI initialized (7 node types registered)");
}

void BehaviorAI::shutdown() {
    FABRIC_LOG_INFO("BehaviorAI shutting down");
    world_ = nullptr;
    initialized_ = false;
}

void BehaviorAI::update(float dt) {
    if (!initialized_ || !world_)
        return;

    auto q = world_->query_builder<BehaviorTreeComponent, AIStateComponent>().build();
    q.each([](BehaviorTreeComponent& btc, AIStateComponent& aiState) {
        if (!btc.tree.rootNode())
            return;

        auto status = btc.tree.tickOnce();

        // Read ai_state written by action nodes via output ports
        int val;
        auto bb = btc.tree.rootBlackboard();
        if (bb && bb->get("ai_state", val)) {
            aiState.state = static_cast<AIState>(val);
        }

        // Reset tree after completion so it re-evaluates next frame
        if (status == BT::NodeStatus::SUCCESS || status == BT::NodeStatus::FAILURE) {
            btc.tree.haltTree();
        }
    });

    // Animation bridge: detect AI state changes and drive blend transitions
    auto animQ = world_->query_builder<AIStateComponent, AIAnimationMapping, AIAnimationState>().build();
    animQ.each([dt](const AIStateComponent& aiState, const AIAnimationMapping& mapping, AIAnimationState& animState) {
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

} // namespace fabric
