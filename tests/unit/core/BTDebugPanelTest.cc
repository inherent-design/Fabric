#include "fabric/core/BTDebugPanel.hh"
#include "fabric/core/BehaviorAI.hh"
#include "fabric/core/ECS.hh"

#include <gtest/gtest.h>

using namespace fabric;

// BTNodeInfo construction and defaults

TEST(BTNodeInfoTest, DefaultConstruction) {
    BTNodeInfo info;
    EXPECT_TRUE(info.name.empty());
    EXPECT_TRUE(info.status.empty());
    EXPECT_EQ(info.depth, 0);
}

TEST(BTNodeInfoTest, ValueConstruction) {
    BTNodeInfo info;
    info.name = "Sequence";
    info.status = "RUNNING";
    info.depth = 2;
    EXPECT_EQ(info.name, "Sequence");
    EXPECT_EQ(info.status, "RUNNING");
    EXPECT_EQ(info.depth, 2);
}

// Observer integration via BehaviorAI

class BTObserverTest : public ::testing::Test {
  protected:
    flecs::world world;
    BehaviorAI ai;

    void SetUp() override { ai.init(world); }
    void TearDown() override { ai.shutdown(); }
};

TEST_F(BTObserverTest, ObserverExistsAfterTick) {
    const char* xml = R"(
        <root BTCPP_format="4">
            <BehaviorTree ID="SimpleTree">
                <PatrolAction ai_state="{ai_state}"/>
            </BehaviorTree>
        </root>
    )";
    auto npc = ai.createNPC(xml);

    // Before tick: no observer yet
    EXPECT_EQ(ai.observerFor(npc), nullptr);

    // After tick: observer exists
    ai.update(0.016f);
    EXPECT_NE(ai.observerFor(npc), nullptr);
}

TEST_F(BTObserverTest, ObserverStatisticsPopulated) {
    const char* xml = R"(
        <root BTCPP_format="4">
            <BehaviorTree ID="Tree">
                <Sequence>
                    <PatrolAction ai_state="{ai_state}"/>
                </Sequence>
            </BehaviorTree>
        </root>
    )";
    auto npc = ai.createNPC(xml);
    ai.update(0.016f);

    auto* obs = ai.observerFor(npc);
    ASSERT_NE(obs, nullptr);

    const auto& stats = obs->statistics();
    EXPECT_GT(stats.size(), 0u);
}

TEST_F(BTObserverTest, ObserverReturnsNullForInvalidEntity) {
    flecs::entity invalid;
    EXPECT_EQ(ai.observerFor(invalid), nullptr);
}

TEST_F(BTObserverTest, ObserverReturnsNullForEntityWithoutTree) {
    auto npc = ai.createNPC("");
    ai.update(0.016f);
    EXPECT_EQ(ai.observerFor(npc), nullptr);
}

TEST_F(BTObserverTest, MultipleNPCsHaveIndependentObservers) {
    const char* xml1 = R"(
        <root BTCPP_format="4">
            <BehaviorTree ID="Tree1">
                <PatrolAction ai_state="{ai_state}"/>
            </BehaviorTree>
        </root>
    )";
    const char* xml2 = R"(
        <root BTCPP_format="4">
            <BehaviorTree ID="Tree2">
                <ChaseAction ai_state="{ai_state}"/>
            </BehaviorTree>
        </root>
    )";
    auto npc1 = ai.createNPC(xml1);
    auto npc2 = ai.createNPC(xml2);
    ai.update(0.016f);

    auto* obs1 = ai.observerFor(npc1);
    auto* obs2 = ai.observerFor(npc2);
    ASSERT_NE(obs1, nullptr);
    ASSERT_NE(obs2, nullptr);
    EXPECT_NE(obs1, obs2);
}

TEST_F(BTObserverTest, ObserverClearedOnShutdown) {
    const char* xml = R"(
        <root BTCPP_format="4">
            <BehaviorTree ID="Tree">
                <PatrolAction ai_state="{ai_state}"/>
            </BehaviorTree>
        </root>
    )";
    auto npc = ai.createNPC(xml);
    ai.update(0.016f);
    ASSERT_NE(ai.observerFor(npc), nullptr);

    ai.shutdown();
    // After shutdown, observerFor should return nullptr
    // (world is also dead, but the map is cleared)
}

// BTDebugPanel lifecycle (without RmlUi context)

TEST(BTDebugPanelTest, DefaultState) {
    BTDebugPanel panel;
    EXPECT_FALSE(panel.isVisible());
}

TEST(BTDebugPanelTest, InitWithNullContext) {
    BTDebugPanel panel;
    panel.init(nullptr); // Should not crash, logs error
    EXPECT_FALSE(panel.isVisible());
}

TEST(BTDebugPanelTest, ToggleWithoutInit) {
    BTDebugPanel panel;
    panel.toggle(); // Should not crash
    EXPECT_TRUE(panel.isVisible());
    panel.toggle();
    EXPECT_FALSE(panel.isVisible());
}

TEST(BTDebugPanelTest, ShutdownWithoutInit) {
    BTDebugPanel panel;
    panel.shutdown(); // Should not crash
    EXPECT_FALSE(panel.isVisible());
}

TEST(BTDebugPanelTest, UpdateWithInvalidEntity) {
    BTDebugPanel panel;
    BehaviorAI ai;
    flecs::world world;
    ai.init(world);

    flecs::entity invalid;
    // Should not crash even without RmlUi init
    panel.update(ai, invalid);

    ai.shutdown();
}

// selectNextNPC cycling

class BTDebugNPCCycleTest : public ::testing::Test {
  protected:
    flecs::world world;
    BehaviorAI ai;

    void SetUp() override { ai.init(world); }
    void TearDown() override { ai.shutdown(); }
};

TEST_F(BTDebugNPCCycleTest, NoNPCsSelectsNothing) {
    BTDebugPanel panel;
    panel.selectNextNPC(ai, world);
    EXPECT_FALSE(panel.selectedNpc().is_valid());
}

TEST_F(BTDebugNPCCycleTest, CyclesThrough3NPCs) {
    const char* xml = R"(
        <root BTCPP_format="4">
            <BehaviorTree ID="Tree">
                <PatrolAction ai_state="{ai_state}"/>
            </BehaviorTree>
        </root>
    )";
    auto npc1 = ai.createNPC(xml);
    auto npc2 = ai.createNPC(xml);
    auto npc3 = ai.createNPC(xml);

    BTDebugPanel panel;

    // First cycle: selects first NPC
    panel.selectNextNPC(ai, world);
    auto first = panel.selectedNpc();
    EXPECT_TRUE(first.is_valid());

    // Collect all three selections
    std::set<uint64_t> seen;
    seen.insert(first.id());

    panel.selectNextNPC(ai, world);
    seen.insert(panel.selectedNpc().id());

    panel.selectNextNPC(ai, world);
    seen.insert(panel.selectedNpc().id());

    // All 3 distinct NPCs visited
    EXPECT_EQ(seen.size(), 3u);

    // Fourth cycle wraps back to first
    panel.selectNextNPC(ai, world);
    EXPECT_EQ(panel.selectedNpc().id(), first.id());
}

TEST_F(BTDebugNPCCycleTest, SkipsNPCsWithoutTree) {
    const char* xml = R"(
        <root BTCPP_format="4">
            <BehaviorTree ID="Tree">
                <PatrolAction ai_state="{ai_state}"/>
            </BehaviorTree>
        </root>
    )";
    ai.createNPC("");              // no tree
    auto npc2 = ai.createNPC(xml); // has tree

    BTDebugPanel panel;
    panel.selectNextNPC(ai, world);

    // Should select the NPC with a tree
    EXPECT_TRUE(panel.selectedNpc().is_valid());
    EXPECT_EQ(panel.selectedNpc().id(), npc2.id());
}

// Observer node traversal

TEST_F(BTObserverTest, ObserverPathToUIDPopulated) {
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

    auto* obs = ai.observerFor(npc);
    ASSERT_NE(obs, nullptr);

    const auto& uidToPath = obs->uidToPath();
    EXPECT_GE(uidToPath.size(), 3u); // Sequence + 2 children

    // Verify at least one path contains node names
    bool foundSequence = false;
    for (const auto& [uid, path] : uidToPath) {
        if (path.find("Sequence") != std::string::npos) {
            foundSequence = true;
        }
    }
    EXPECT_TRUE(foundSequence);
}
