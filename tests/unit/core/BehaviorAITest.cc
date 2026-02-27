#include "fabric/core/BehaviorAI.hh"

#include <gtest/gtest.h>

using namespace fabric;

class BehaviorAITest : public ::testing::Test {
  protected:
    flecs::world world;
    BehaviorAI ai;

    void SetUp() override { ai.init(world); }

    void TearDown() override { ai.shutdown(); }
};

// Lifecycle

TEST_F(BehaviorAITest, InitAndShutdown) {
    BehaviorAI ai2;
    flecs::world w2;
    ai2.init(w2);
    ai2.shutdown();
}

TEST_F(BehaviorAITest, UpdateWithoutNPCs) {
    ai.update(0.016f);
}

// NPC creation

TEST_F(BehaviorAITest, CreateNPCWithoutTree) {
    auto npc = ai.createNPC("");
    EXPECT_TRUE(npc.has<NPCTag>());
    EXPECT_TRUE(npc.has<AIStateComponent>());
    EXPECT_FALSE(npc.has<BehaviorTreeComponent>());

    const auto& state = npc.get<AIStateComponent>();
    EXPECT_EQ(state.state, AIState::Idle);
}

TEST_F(BehaviorAITest, CreateNPCWithTree) {
    const char* xml = R"(
        <root BTCPP_format="4">
            <BehaviorTree ID="SimpleTree">
                <PatrolAction ai_state="{ai_state}"/>
            </BehaviorTree>
        </root>
    )";
    auto npc = ai.createNPC(xml);
    EXPECT_TRUE(npc.has<NPCTag>());
    EXPECT_TRUE(npc.has<AIStateComponent>());
    EXPECT_TRUE(npc.has<BehaviorTreeComponent>());
}

// Tree loading

TEST_F(BehaviorAITest, LoadBehaviorTree) {
    const char* xml = R"(
        <root BTCPP_format="4">
            <BehaviorTree ID="TestTree">
                <PatrolAction ai_state="{ai_state}"/>
            </BehaviorTree>
        </root>
    )";
    auto tree = ai.loadBehaviorTree(xml);
    EXPECT_NE(tree.rootNode(), nullptr);
}

// Action nodes set AIState

TEST_F(BehaviorAITest, PatrolActionSetsState) {
    const char* xml = R"(
        <root BTCPP_format="4">
            <BehaviorTree ID="Tree">
                <PatrolAction ai_state="{ai_state}"/>
            </BehaviorTree>
        </root>
    )";
    auto npc = ai.createNPC(xml);
    ai.update(0.016f);

    const auto& state = npc.get<AIStateComponent>();
    EXPECT_EQ(state.state, AIState::Patrol);
}

TEST_F(BehaviorAITest, ChaseActionSetsState) {
    const char* xml = R"(
        <root BTCPP_format="4">
            <BehaviorTree ID="Tree">
                <ChaseAction ai_state="{ai_state}"/>
            </BehaviorTree>
        </root>
    )";
    auto npc = ai.createNPC(xml);
    ai.update(0.016f);

    const auto& state = npc.get<AIStateComponent>();
    EXPECT_EQ(state.state, AIState::Chase);
}

TEST_F(BehaviorAITest, AttackActionSetsState) {
    const char* xml = R"(
        <root BTCPP_format="4">
            <BehaviorTree ID="Tree">
                <AttackAction ai_state="{ai_state}"/>
            </BehaviorTree>
        </root>
    )";
    auto npc = ai.createNPC(xml);
    ai.update(0.016f);

    const auto& state = npc.get<AIStateComponent>();
    EXPECT_EQ(state.state, AIState::Attack);
}

TEST_F(BehaviorAITest, FleeActionSetsState) {
    const char* xml = R"(
        <root BTCPP_format="4">
            <BehaviorTree ID="Tree">
                <FleeAction ai_state="{ai_state}"/>
            </BehaviorTree>
        </root>
    )";
    auto npc = ai.createNPC(xml);
    ai.update(0.016f);

    const auto& state = npc.get<AIStateComponent>();
    EXPECT_EQ(state.state, AIState::Flee);
}

// Condition nodes

TEST_F(BehaviorAITest, IsPlayerNearbyTrueTriggersChase) {
    const char* xml = R"(
        <root BTCPP_format="4">
            <BehaviorTree ID="Tree">
                <Sequence>
                    <IsPlayerNearby player_distance="{player_distance}" detection_range="10.0"/>
                    <ChaseAction ai_state="{ai_state}"/>
                </Sequence>
            </BehaviorTree>
        </root>
    )";
    auto npc = ai.createNPC(xml);

    auto& btc = npc.get_mut<BehaviorTreeComponent>();
    btc.tree.rootBlackboard()->set("player_distance", 5.0f);

    ai.update(0.016f);

    const auto& state = npc.get<AIStateComponent>();
    EXPECT_EQ(state.state, AIState::Chase);
}

TEST_F(BehaviorAITest, IsPlayerNearbyFalseSkipsAction) {
    const char* xml = R"(
        <root BTCPP_format="4">
            <BehaviorTree ID="Tree">
                <Sequence>
                    <IsPlayerNearby player_distance="{player_distance}" detection_range="10.0"/>
                    <ChaseAction ai_state="{ai_state}"/>
                </Sequence>
            </BehaviorTree>
        </root>
    )";
    auto npc = ai.createNPC(xml);

    auto& btc = npc.get_mut<BehaviorTreeComponent>();
    btc.tree.rootBlackboard()->set("player_distance", 50.0f);

    ai.update(0.016f);

    const auto& state = npc.get<AIStateComponent>();
    EXPECT_EQ(state.state, AIState::Idle);
}

TEST_F(BehaviorAITest, IsHealthLowTriggersFlee) {
    const char* xml = R"(
        <root BTCPP_format="4">
            <BehaviorTree ID="Tree">
                <Sequence>
                    <IsHealthLow health="{health}" health_threshold="30.0"/>
                    <FleeAction ai_state="{ai_state}"/>
                </Sequence>
            </BehaviorTree>
        </root>
    )";
    auto npc = ai.createNPC(xml);

    auto& btc = npc.get_mut<BehaviorTreeComponent>();
    btc.tree.rootBlackboard()->set("health", 20.0f);

    ai.update(0.016f);

    const auto& state = npc.get<AIStateComponent>();
    EXPECT_EQ(state.state, AIState::Flee);
}

TEST_F(BehaviorAITest, IsHealthHighSkipsFlee) {
    const char* xml = R"(
        <root BTCPP_format="4">
            <BehaviorTree ID="Tree">
                <Sequence>
                    <IsHealthLow health="{health}" health_threshold="30.0"/>
                    <FleeAction ai_state="{ai_state}"/>
                </Sequence>
            </BehaviorTree>
        </root>
    )";
    auto npc = ai.createNPC(xml);

    auto& btc = npc.get_mut<BehaviorTreeComponent>();
    btc.tree.rootBlackboard()->set("health", 100.0f);

    ai.update(0.016f);

    const auto& state = npc.get<AIStateComponent>();
    EXPECT_EQ(state.state, AIState::Idle);
}

TEST_F(BehaviorAITest, HasTargetTrueTriggersAttack) {
    const char* xml = R"(
        <root BTCPP_format="4">
            <BehaviorTree ID="Tree">
                <Sequence>
                    <HasTarget has_target="{has_target}"/>
                    <AttackAction ai_state="{ai_state}"/>
                </Sequence>
            </BehaviorTree>
        </root>
    )";
    auto npc = ai.createNPC(xml);

    auto& btc = npc.get_mut<BehaviorTreeComponent>();
    btc.tree.rootBlackboard()->set("has_target", true);

    ai.update(0.016f);

    const auto& state = npc.get<AIStateComponent>();
    EXPECT_EQ(state.state, AIState::Attack);
}

TEST_F(BehaviorAITest, HasTargetFalseSkipsAttack) {
    const char* xml = R"(
        <root BTCPP_format="4">
            <BehaviorTree ID="Tree">
                <Sequence>
                    <HasTarget has_target="{has_target}"/>
                    <AttackAction ai_state="{ai_state}"/>
                </Sequence>
            </BehaviorTree>
        </root>
    )";
    auto npc = ai.createNPC(xml);

    auto& btc = npc.get_mut<BehaviorTreeComponent>();
    btc.tree.rootBlackboard()->set("has_target", false);

    ai.update(0.016f);

    const auto& state = npc.get<AIStateComponent>();
    EXPECT_EQ(state.state, AIState::Idle);
}

// Complex behavior tree with fallback priority

TEST_F(BehaviorAITest, FallbackPatrolsWhenHealthyAndNoPlayer) {
    const char* xml = R"(
        <root BTCPP_format="4">
            <BehaviorTree ID="NPCBrain">
                <Fallback>
                    <Sequence>
                        <IsHealthLow health="{health}" health_threshold="30.0"/>
                        <FleeAction ai_state="{ai_state}"/>
                    </Sequence>
                    <Sequence>
                        <IsPlayerNearby player_distance="{player_distance}" detection_range="10.0"/>
                        <ChaseAction ai_state="{ai_state}"/>
                    </Sequence>
                    <PatrolAction ai_state="{ai_state}"/>
                </Fallback>
            </BehaviorTree>
        </root>
    )";
    auto npc = ai.createNPC(xml);

    auto& btc = npc.get_mut<BehaviorTreeComponent>();
    btc.tree.rootBlackboard()->set("health", 100.0f);
    btc.tree.rootBlackboard()->set("player_distance", 50.0f);

    ai.update(0.016f);

    const auto& state = npc.get<AIStateComponent>();
    EXPECT_EQ(state.state, AIState::Patrol);
}

TEST_F(BehaviorAITest, FallbackFleesOnLowHealth) {
    const char* xml = R"(
        <root BTCPP_format="4">
            <BehaviorTree ID="NPCBrain">
                <Fallback>
                    <Sequence>
                        <IsHealthLow health="{health}" health_threshold="30.0"/>
                        <FleeAction ai_state="{ai_state}"/>
                    </Sequence>
                    <Sequence>
                        <IsPlayerNearby player_distance="{player_distance}" detection_range="10.0"/>
                        <ChaseAction ai_state="{ai_state}"/>
                    </Sequence>
                    <PatrolAction ai_state="{ai_state}"/>
                </Fallback>
            </BehaviorTree>
        </root>
    )";
    auto npc = ai.createNPC(xml);

    auto& btc = npc.get_mut<BehaviorTreeComponent>();
    btc.tree.rootBlackboard()->set("health", 10.0f);
    btc.tree.rootBlackboard()->set("player_distance", 5.0f);

    ai.update(0.016f);

    // Low health takes priority over player nearby
    const auto& state = npc.get<AIStateComponent>();
    EXPECT_EQ(state.state, AIState::Flee);
}

TEST_F(BehaviorAITest, FallbackChasesWhenHealthyAndPlayerNear) {
    const char* xml = R"(
        <root BTCPP_format="4">
            <BehaviorTree ID="NPCBrain">
                <Fallback>
                    <Sequence>
                        <IsHealthLow health="{health}" health_threshold="30.0"/>
                        <FleeAction ai_state="{ai_state}"/>
                    </Sequence>
                    <Sequence>
                        <IsPlayerNearby player_distance="{player_distance}" detection_range="10.0"/>
                        <ChaseAction ai_state="{ai_state}"/>
                    </Sequence>
                    <PatrolAction ai_state="{ai_state}"/>
                </Fallback>
            </BehaviorTree>
        </root>
    )";
    auto npc = ai.createNPC(xml);

    auto& btc = npc.get_mut<BehaviorTreeComponent>();
    btc.tree.rootBlackboard()->set("health", 80.0f);
    btc.tree.rootBlackboard()->set("player_distance", 5.0f);

    ai.update(0.016f);

    const auto& state = npc.get<AIStateComponent>();
    EXPECT_EQ(state.state, AIState::Chase);
}

// Multiple NPCs

TEST_F(BehaviorAITest, MultipleNPCsIndependentState) {
    const char* patrolXml = R"(
        <root BTCPP_format="4">
            <BehaviorTree ID="PatrolTree">
                <PatrolAction ai_state="{ai_state}"/>
            </BehaviorTree>
        </root>
    )";
    const char* chaseXml = R"(
        <root BTCPP_format="4">
            <BehaviorTree ID="ChaseTree">
                <ChaseAction ai_state="{ai_state}"/>
            </BehaviorTree>
        </root>
    )";

    auto npc1 = ai.createNPC(patrolXml);
    auto npc2 = ai.createNPC(chaseXml);

    ai.update(0.016f);

    EXPECT_EQ(npc1.get<AIStateComponent>().state, AIState::Patrol);
    EXPECT_EQ(npc2.get<AIStateComponent>().state, AIState::Chase);
}

// Repeated updates re-evaluate the tree

TEST_F(BehaviorAITest, RepeatedUpdatesReEvaluate) {
    const char* xml = R"(
        <root BTCPP_format="4">
            <BehaviorTree ID="Tree">
                <Sequence>
                    <IsPlayerNearby player_distance="{player_distance}" detection_range="10.0"/>
                    <ChaseAction ai_state="{ai_state}"/>
                </Sequence>
            </BehaviorTree>
        </root>
    )";
    auto npc = ai.createNPC(xml);
    auto& btc = npc.get_mut<BehaviorTreeComponent>();

    // Player far away: stays idle
    btc.tree.rootBlackboard()->set("player_distance", 50.0f);
    ai.update(0.016f);
    EXPECT_EQ(npc.get<AIStateComponent>().state, AIState::Idle);

    // Player moves closer: transitions to chase
    btc.tree.rootBlackboard()->set("player_distance", 3.0f);
    ai.update(0.016f);
    EXPECT_EQ(npc.get<AIStateComponent>().state, AIState::Chase);
}

// Factory access

TEST_F(BehaviorAITest, FactoryHasRegisteredNodes) {
    auto& fac = ai.factory();
    const auto& manifests = fac.manifests();

    bool hasPatrol = false;
    bool hasChase = false;
    bool hasIsPlayerNearby = false;
    for (const auto& [id, manifest] : manifests) {
        if (id == "PatrolAction")
            hasPatrol = true;
        if (id == "ChaseAction")
            hasChase = true;
        if (id == "IsPlayerNearby")
            hasIsPlayerNearby = true;
    }
    EXPECT_TRUE(hasPatrol);
    EXPECT_TRUE(hasChase);
    EXPECT_TRUE(hasIsPlayerNearby);
}

// Animation bridge tests

TEST_F(BehaviorAITest, SetAnimationMapping) {
    auto npc = ai.createNPC("");
    AIAnimationMapping mapping;
    ai.setAnimationMapping(npc, mapping);

    EXPECT_TRUE(npc.has<AIAnimationMapping>());
    EXPECT_TRUE(npc.has<AIAnimationState>());
}

TEST_F(BehaviorAITest, AIStateChangeTriggerBlend) {
    const char* xml = R"(
        <root BTCPP_format="4">
            <BehaviorTree ID="Tree">
                <ChaseAction ai_state="{ai_state}"/>
            </BehaviorTree>
        </root>
    )";
    auto npc = ai.createNPC(xml);
    AIAnimationMapping mapping;
    ai.setAnimationMapping(npc, mapping);

    ai.update(0.016f);

    const auto& animState = npc.get<AIAnimationState>();
    EXPECT_TRUE(animState.blending);
    EXPECT_EQ(animState.previousState, AIState::Chase);
}

TEST_F(BehaviorAITest, BlendTimerAdvances) {
    auto npc = ai.createNPC("");
    AIAnimationMapping mapping;
    mapping.blendDuration = 1.0f;
    ai.setAnimationMapping(npc, mapping);

    // Force a state change to start blending
    npc.set<AIStateComponent>({AIState::Patrol});
    ai.update(0.016f);

    const auto& state1 = npc.get<AIAnimationState>();
    float t1 = state1.blendTimer;
    EXPECT_GT(t1, 0.0f);

    ai.update(0.016f);
    const auto& state2 = npc.get<AIAnimationState>();
    EXPECT_GT(state2.blendTimer, t1);
}

TEST_F(BehaviorAITest, BlendCompletes) {
    auto npc = ai.createNPC("");
    AIAnimationMapping mapping;
    mapping.blendDuration = 0.05f;
    ai.setAnimationMapping(npc, mapping);

    // Trigger a state change
    npc.set<AIStateComponent>({AIState::Attack});
    ai.update(0.016f);

    const auto& mid = npc.get<AIAnimationState>();
    EXPECT_TRUE(mid.blending);

    // Advance enough to complete blend
    ai.update(0.02f);
    ai.update(0.02f);

    const auto& done = npc.get<AIAnimationState>();
    EXPECT_FALSE(done.blending);
}

TEST_F(BehaviorAITest, GetClipNameForState) {
    AIAnimationMapping mapping;
    EXPECT_EQ(ai.getClipNameForState(mapping, AIState::Idle), "idle");
    EXPECT_EQ(ai.getClipNameForState(mapping, AIState::Patrol), "walk");
    EXPECT_EQ(ai.getClipNameForState(mapping, AIState::Chase), "run");
    EXPECT_EQ(ai.getClipNameForState(mapping, AIState::Attack), "attack");
    EXPECT_EQ(ai.getClipNameForState(mapping, AIState::Flee), "run_fast");
}

TEST_F(BehaviorAITest, CustomMappingOverride) {
    AIAnimationMapping mapping;
    mapping.idleClip = "custom_idle";
    mapping.patrolClip = "custom_walk";
    mapping.chaseClip = "custom_run";
    mapping.attackClip = "custom_attack";
    mapping.fleeClip = "custom_flee";

    EXPECT_EQ(ai.getClipNameForState(mapping, AIState::Idle), "custom_idle");
    EXPECT_EQ(ai.getClipNameForState(mapping, AIState::Patrol), "custom_walk");
    EXPECT_EQ(ai.getClipNameForState(mapping, AIState::Chase), "custom_run");
    EXPECT_EQ(ai.getClipNameForState(mapping, AIState::Attack), "custom_attack");
    EXPECT_EQ(ai.getClipNameForState(mapping, AIState::Flee), "custom_flee");
}

TEST_F(BehaviorAITest, NoMappingDoesNotCrash) {
    auto npc = ai.createNPC("");
    EXPECT_FALSE(npc.has<AIAnimationMapping>());
    ai.update(0.016f);
    ai.update(0.016f);
}
