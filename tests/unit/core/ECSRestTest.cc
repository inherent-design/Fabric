#include "fabric/core/ECS.hh"

#include <gtest/gtest.h>

using namespace fabric;

#ifdef FABRIC_ECS_INSPECTOR

TEST(ECSRestTest, EnableInspectorSetsRestSingleton) {
    World world;
    world.registerCoreComponents();

    EXPECT_FALSE(world.isInspectorEnabled());

    uint16_t port = world.enableInspector();

    EXPECT_TRUE(world.isInspectorEnabled());
    EXPECT_EQ(port, ECS_REST_DEFAULT_PORT);
}

TEST(ECSRestTest, EnableInspectorCustomPort) {
    World world;
    world.registerCoreComponents();

    constexpr uint16_t kCustomPort = 9090;
    uint16_t port = world.enableInspector(kCustomPort);

    EXPECT_TRUE(world.isInspectorEnabled());
    EXPECT_EQ(port, kCustomPort);
}

TEST(ECSRestTest, EnableInspectorDefaultPort) {
    World world;
    world.registerCoreComponents();

    uint16_t port = world.enableInspector(0);

    EXPECT_EQ(port, ECS_REST_DEFAULT_PORT);
}

TEST(ECSRestTest, StatsModuleImported) {
    World world;
    world.registerCoreComponents();
    world.enableInspector();

    // The stats module registers WorldSummary as a component.
    // Verify it exists by checking that the C++ component ID resolves.
    auto id = world.get().component<flecs::WorldSummary>();
    EXPECT_TRUE(id.is_valid());
}

TEST(ECSRestTest, CoreComponentsVisibleWithInspector) {
    World world;
    world.registerCoreComponents();
    world.enableInspector();

    // Core components should still be queryable after enabling inspector
    auto posComp = world.get().lookup("Position");
    auto rotComp = world.get().lookup("Rotation");
    auto scaleComp = world.get().lookup("Scale");
    auto bbComp = world.get().lookup("BoundingBox");

    EXPECT_TRUE(posComp.is_valid());
    EXPECT_TRUE(rotComp.is_valid());
    EXPECT_TRUE(scaleComp.is_valid());
    EXPECT_TRUE(bbComp.is_valid());
}

TEST(ECSRestTest, EntityHierarchyWithInspector) {
    World world;
    world.registerCoreComponents();
    world.enableInspector();

    auto parent = world.createSceneEntity("parent");
    auto child = world.createChildEntity(parent, "child");

    // ChildOf relationship should work with REST module active
    EXPECT_TRUE(child.has(flecs::ChildOf, parent));
    EXPECT_EQ(child.parent(), parent);

    // Entities should be findable (REST explorer uses entity lookup)
    auto found = world.get().lookup("parent");
    EXPECT_TRUE(found.is_valid());
    EXPECT_EQ(found, parent);
}

#else // !FABRIC_ECS_INSPECTOR

TEST(ECSRestTest, InspectorDisabledReturnsZero) {
    World world;
    world.registerCoreComponents();

    uint16_t port = world.enableInspector();

    EXPECT_EQ(port, 0);
    EXPECT_FALSE(world.isInspectorEnabled());
}

#endif // FABRIC_ECS_INSPECTOR
