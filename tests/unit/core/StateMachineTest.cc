#include "fabric/core/StateMachine.hh"
#include "fabric/utils/ErrorHandling.hh"
#include <atomic>
#include <gtest/gtest.h>
#include <thread>
#include <vector>

using namespace fabric;

enum class ConnectionState {
    Disconnected,
    Connecting,
    Connected,
    Draining,
    Closed
};

static std::string connectionStateToString(ConnectionState state) {
    switch (state) {
        case ConnectionState::Disconnected:
            return "Disconnected";
        case ConnectionState::Connecting:
            return "Connecting";
        case ConnectionState::Connected:
            return "Connected";
        case ConnectionState::Draining:
            return "Draining";
        case ConnectionState::Closed:
            return "Closed";
        default:
            return "Unknown";
    }
}

class StateMachineTest : public ::testing::Test {
  protected:
    void SetUp() override {
        sm = std::make_unique<StateMachine<ConnectionState>>(ConnectionState::Disconnected, connectionStateToString);

        sm->addTransition(ConnectionState::Disconnected, ConnectionState::Connecting);
        sm->addTransition(ConnectionState::Connecting, ConnectionState::Connected);
        sm->addTransition(ConnectionState::Connected, ConnectionState::Draining);
        sm->addTransition(ConnectionState::Draining, ConnectionState::Closed);
        sm->addTransition(ConnectionState::Connected, ConnectionState::Closed);
        sm->addTransition(ConnectionState::Disconnected, ConnectionState::Closed);
    }

    std::unique_ptr<StateMachine<ConnectionState>> sm;
};

TEST_F(StateMachineTest, ValidTransitions) {
    EXPECT_EQ(sm->getState(), ConnectionState::Disconnected);

    sm->setState(ConnectionState::Connecting);
    EXPECT_EQ(sm->getState(), ConnectionState::Connecting);

    sm->setState(ConnectionState::Connected);
    EXPECT_EQ(sm->getState(), ConnectionState::Connected);

    sm->setState(ConnectionState::Draining);
    EXPECT_EQ(sm->getState(), ConnectionState::Draining);

    sm->setState(ConnectionState::Closed);
    EXPECT_EQ(sm->getState(), ConnectionState::Closed);
}

TEST_F(StateMachineTest, InvalidTransitionsThrow) {
    EXPECT_THROW(sm->setState(ConnectionState::Connected), FabricException);
    EXPECT_THROW(sm->setState(ConnectionState::Draining), FabricException);
}

TEST_F(StateMachineTest, SelfTransitionsAreNoOps) {
    int hookCalls = 0;
    sm->addHook(ConnectionState::Disconnected, [&hookCalls]() { hookCalls++; });

    sm->setState(ConnectionState::Disconnected);
    EXPECT_EQ(sm->getState(), ConnectionState::Disconnected);
    EXPECT_EQ(hookCalls, 0);
}

TEST_F(StateMachineTest, StateHooksFire) {
    int hookCalls = 0;
    sm->addHook(ConnectionState::Connecting, [&hookCalls]() { hookCalls++; });

    sm->setState(ConnectionState::Connecting);
    EXPECT_EQ(hookCalls, 1);

    sm->setState(ConnectionState::Connected);
    EXPECT_EQ(hookCalls, 1);
}

TEST_F(StateMachineTest, TransitionHooksFire) {
    int hookCalls = 0;
    sm->addTransitionHook(ConnectionState::Connecting, ConnectionState::Connected, [&hookCalls]() { hookCalls++; });

    sm->setState(ConnectionState::Connecting);
    EXPECT_EQ(hookCalls, 0);

    sm->setState(ConnectionState::Connected);
    EXPECT_EQ(hookCalls, 1);
}

TEST_F(StateMachineTest, HookRemoval) {
    int hookCalls = 0;
    auto id = sm->addHook(ConnectionState::Connecting, [&hookCalls]() { hookCalls++; });

    EXPECT_TRUE(sm->removeHook(id));
    EXPECT_FALSE(sm->removeHook(id));
    EXPECT_FALSE(sm->removeHook("nonexistent"));

    sm->setState(ConnectionState::Connecting);
    EXPECT_EQ(hookCalls, 0);
}

TEST_F(StateMachineTest, TransitionHookRemoval) {
    int hookCalls = 0;
    auto id =
        sm->addTransitionHook(ConnectionState::Connecting, ConnectionState::Connected, [&hookCalls]() { hookCalls++; });

    EXPECT_TRUE(sm->removeHook(id));
    EXPECT_FALSE(sm->removeHook(id));

    sm->setState(ConnectionState::Connecting);
    sm->setState(ConnectionState::Connected);
    EXPECT_EQ(hookCalls, 0);
}

TEST_F(StateMachineTest, ThreadSafety) {
    sm->setState(ConnectionState::Connecting);
    sm->setState(ConnectionState::Connected);

    // Allow cycling between Connected and Draining
    sm->addTransition(ConnectionState::Draining, ConnectionState::Connected);

    std::vector<std::thread> threads;
    for (int i = 0; i < 4; ++i) {
        threads.emplace_back([this]() {
            for (int j = 0; j < 100; ++j) {
                try {
                    sm->setState(ConnectionState::Draining);
                } catch (...) {}
                try {
                    sm->setState(ConnectionState::Connected);
                } catch (...) {}
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    auto state = sm->getState();
    EXPECT_TRUE(state == ConnectionState::Connected || state == ConnectionState::Draining);
}

TEST_F(StateMachineTest, GetStateReturnsCorrectState) {
    EXPECT_EQ(sm->getState(), ConnectionState::Disconnected);

    sm->setState(ConnectionState::Connecting);
    EXPECT_EQ(sm->getState(), ConnectionState::Connecting);

    sm->setState(ConnectionState::Connected);
    EXPECT_EQ(sm->getState(), ConnectionState::Connected);
}

TEST_F(StateMachineTest, IsValidTransition) {
    EXPECT_TRUE(sm->isValidTransition(ConnectionState::Disconnected, ConnectionState::Connecting));
    EXPECT_TRUE(sm->isValidTransition(ConnectionState::Connecting, ConnectionState::Connected));
    EXPECT_TRUE(sm->isValidTransition(ConnectionState::Connected, ConnectionState::Draining));

    // Self-transitions are always valid
    EXPECT_TRUE(sm->isValidTransition(ConnectionState::Disconnected, ConnectionState::Disconnected));
    EXPECT_TRUE(sm->isValidTransition(ConnectionState::Closed, ConnectionState::Closed));

    // Invalid transitions
    EXPECT_FALSE(sm->isValidTransition(ConnectionState::Disconnected, ConnectionState::Connected));
    EXPECT_FALSE(sm->isValidTransition(ConnectionState::Closed, ConnectionState::Disconnected));
}

TEST_F(StateMachineTest, NullHookThrows) {
    EXPECT_THROW(sm->addHook(ConnectionState::Connecting, nullptr), FabricException);
    EXPECT_THROW(sm->addTransitionHook(ConnectionState::Disconnected, ConnectionState::Connecting, nullptr),
                 FabricException);
}

TEST_F(StateMachineTest, MultipleHooksOnSameState) {
    int hook1 = 0;
    int hook2 = 0;

    sm->addHook(ConnectionState::Connecting, [&hook1]() { hook1++; });
    sm->addHook(ConnectionState::Connecting, [&hook2]() { hook2++; });

    sm->setState(ConnectionState::Connecting);
    EXPECT_EQ(hook1, 1);
    EXPECT_EQ(hook2, 1);
}
