#include "fabric/core/WorldLifecycle.hh"
#include "fabric/ecs/ECS.hh"
#include "fabric/ecs/WorldScoped.hh"

#include <gtest/gtest.h>
#include <string>
#include <vector>

using namespace fabric;

class WorldLifecycleTest : public ::testing::Test {
  protected:
    WorldLifecycleCoordinator coordinator;
    World world;
    std::vector<std::string> callOrder;
};

// --- Unit tests: coordinator mechanics ---

TEST_F(WorldLifecycleTest, RegisterSingleParticipant) {
    bool beginCalled = false;
    bool endCalled = false;
    coordinator.registerParticipant([&]() { beginCalled = true; }, [&]() { endCalled = true; });

    coordinator.setWorld(world);
    coordinator.beginWorld();
    EXPECT_TRUE(beginCalled);
    EXPECT_FALSE(endCalled);

    coordinator.endWorld();
    EXPECT_TRUE(endCalled);
}

TEST_F(WorldLifecycleTest, RegisterMultipleParticipants) {
    constexpr int N = 5;
    int beginCount = 0;
    int endCount = 0;

    for (int i = 0; i < N; ++i) {
        coordinator.registerParticipant([&]() { ++beginCount; }, [&]() { ++endCount; });
    }

    EXPECT_EQ(coordinator.participantCount(), static_cast<size_t>(N));

    coordinator.setWorld(world);
    coordinator.beginWorld();
    EXPECT_EQ(beginCount, N);

    coordinator.endWorld();
    EXPECT_EQ(endCount, N);
}

TEST_F(WorldLifecycleTest, BeginWorldDispatchesInRegistrationOrder) {
    coordinator.registerParticipant([&]() { callOrder.push_back("A"); }, nullptr);
    coordinator.registerParticipant([&]() { callOrder.push_back("B"); }, nullptr);
    coordinator.registerParticipant([&]() { callOrder.push_back("C"); }, nullptr);

    coordinator.setWorld(world);
    coordinator.beginWorld();

    ASSERT_EQ(callOrder.size(), 3u);
    EXPECT_EQ(callOrder[0], "A");
    EXPECT_EQ(callOrder[1], "B");
    EXPECT_EQ(callOrder[2], "C");
}

TEST_F(WorldLifecycleTest, EndWorldDispatchesInReverseOrder) {
    coordinator.registerParticipant(nullptr, [&]() { callOrder.push_back("A"); });
    coordinator.registerParticipant(nullptr, [&]() { callOrder.push_back("B"); });
    coordinator.registerParticipant(nullptr, [&]() { callOrder.push_back("C"); });

    coordinator.setWorld(world);
    coordinator.endWorld();

    ASSERT_EQ(callOrder.size(), 3u);
    EXPECT_EQ(callOrder[0], "C");
    EXPECT_EQ(callOrder[1], "B");
    EXPECT_EQ(callOrder[2], "A");
}

TEST_F(WorldLifecycleTest, EndWorldDeletesWorldScopedEntities) {
    coordinator.setWorld(world);
    world.registerCoreComponents();

    auto& ecs = world.get();
    ecs.component<WorldScoped>();

    auto e1 = ecs.entity().add<WorldScoped>();
    auto e2 = ecs.entity().add<WorldScoped>();
    auto e3 = ecs.entity(); // not WorldScoped

    EXPECT_TRUE(e1.is_alive());
    EXPECT_TRUE(e2.is_alive());
    EXPECT_TRUE(e3.is_alive());

    coordinator.endWorld();

    EXPECT_FALSE(e1.is_alive());
    EXPECT_FALSE(e2.is_alive());
    EXPECT_TRUE(e3.is_alive());
}

TEST_F(WorldLifecycleTest, BeginAfterEndResetsState) {
    int beginCount = 0;
    int endCount = 0;

    coordinator.registerParticipant([&]() { ++beginCount; }, [&]() { ++endCount; });
    coordinator.setWorld(world);

    coordinator.beginWorld();
    EXPECT_EQ(beginCount, 1);
    coordinator.endWorld();
    EXPECT_EQ(endCount, 1);

    coordinator.beginWorld();
    EXPECT_EQ(beginCount, 2);
    coordinator.endWorld();
    EXPECT_EQ(endCount, 2);
}

TEST_F(WorldLifecycleTest, EmptyCoordinatorSafe) {
    coordinator.setWorld(world);
    EXPECT_EQ(coordinator.participantCount(), 0u);

    // Neither should crash
    coordinator.beginWorld();
    coordinator.endWorld();
}

TEST_F(WorldLifecycleTest, NullCallbacksSafe) {
    coordinator.registerParticipant(nullptr, nullptr);
    coordinator.setWorld(world);

    // Should not crash even with null callbacks
    coordinator.beginWorld();
    coordinator.endWorld();
}

// --- Integration-style tests: state reset verification ---

TEST_F(WorldLifecycleTest, WorldTransitionClearsCallbackState) {
    coordinator.registerParticipant([&]() { callOrder.push_back("begin"); }, [&]() { callOrder.push_back("end"); });
    coordinator.setWorld(world);

    // Load
    coordinator.beginWorld();
    ASSERT_EQ(callOrder.size(), 1u);
    EXPECT_EQ(callOrder[0], "begin");

    // Unload
    coordinator.endWorld();
    ASSERT_EQ(callOrder.size(), 2u);
    EXPECT_EQ(callOrder[1], "end");

    // Reload
    coordinator.beginWorld();
    ASSERT_EQ(callOrder.size(), 3u);
    EXPECT_EQ(callOrder[2], "begin");
}

TEST_F(WorldLifecycleTest, MultipleTransitionsStable) {
    int beginCount = 0;
    int endCount = 0;

    coordinator.registerParticipant([&]() { ++beginCount; }, [&]() { ++endCount; });
    coordinator.setWorld(world);

    for (int cycle = 1; cycle <= 3; ++cycle) {
        coordinator.beginWorld();
        EXPECT_EQ(beginCount, cycle);
        coordinator.endWorld();
        EXPECT_EQ(endCount, cycle);
    }
}

TEST_F(WorldLifecycleTest, MultipleTransitionsPreserveWorldScopedCleanup) {
    coordinator.setWorld(world);
    world.registerCoreComponents();

    auto& ecs = world.get();
    ecs.component<WorldScoped>();

    for (int cycle = 0; cycle < 3; ++cycle) {
        coordinator.beginWorld();

        auto e = ecs.entity().add<WorldScoped>();
        EXPECT_TRUE(e.is_alive());

        coordinator.endWorld();
        EXPECT_FALSE(e.is_alive());
    }
}

// --- Regression tests ---

TEST_F(WorldLifecycleTest, NoDuplicateCallbacksOnReregister) {
    int count = 0;
    auto cb = [&]() {
        ++count;
    };

    coordinator.registerParticipant(cb, nullptr);
    coordinator.registerParticipant(cb, nullptr);

    EXPECT_EQ(coordinator.participantCount(), 2u);

    coordinator.setWorld(world);
    coordinator.beginWorld();
    EXPECT_EQ(count, 2);
}

TEST_F(WorldLifecycleTest, EndWorldBeforeBeginWorldSafe) {
    coordinator.registerParticipant(nullptr, [&]() { callOrder.push_back("end"); });
    coordinator.setWorld(world);

    // Calling endWorld without prior beginWorld should not crash
    coordinator.endWorld();
    ASSERT_EQ(callOrder.size(), 1u);
    EXPECT_EQ(callOrder[0], "end");
}

TEST_F(WorldLifecycleTest, EndWorldWithoutSetWorldSafe) {
    coordinator.registerParticipant(nullptr, [&]() { callOrder.push_back("end"); });

    // No setWorld call; world_ is nullptr. endWorld should still dispatch callbacks
    // but skip the delete_with<WorldScoped> call.
    coordinator.endWorld();
    ASSERT_EQ(callOrder.size(), 1u);
    EXPECT_EQ(callOrder[0], "end");
}

TEST_F(WorldLifecycleTest, ParticipantCountAccurate) {
    EXPECT_EQ(coordinator.participantCount(), 0u);

    coordinator.registerParticipant(nullptr, nullptr);
    EXPECT_EQ(coordinator.participantCount(), 1u);

    coordinator.registerParticipant(nullptr, nullptr);
    EXPECT_EQ(coordinator.participantCount(), 2u);
}
