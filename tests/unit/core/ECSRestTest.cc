#include "fabric/core/ECS.hh"

#include <gtest/gtest.h>

using namespace fabric;

#ifdef FABRIC_ECS_INSPECTOR

TEST(ECSRestTest, EnableInspectorSetsRestSingleton) {
    World world;
    world.registerCoreComponents();
    world.enableInspector();

    // The REST singleton should be present after enableInspector()
    EXPECT_TRUE(world.get().has<flecs::Rest>());
}

TEST(ECSRestTest, EnableInspectorDefaultPort) {
    World world;
    world.registerCoreComponents();
    world.enableInspector();

    ASSERT_TRUE(world.get().has<flecs::Rest>());
    const auto& rest = world.get().get<flecs::Rest>();
    // Default REST port is 27750
    EXPECT_EQ(rest.port, 27750);
}

#else

TEST(ECSRestTest, InspectorDisabledInRelease) {
    // FABRIC_ECS_INSPECTOR not defined: enableInspector() should not exist.
    // This test verifies the World compiles without inspector support.
    World world;
    world.registerCoreComponents();
    EXPECT_TRUE(world.get().get_world() != nullptr);
}

#endif
