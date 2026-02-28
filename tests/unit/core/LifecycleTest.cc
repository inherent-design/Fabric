#include "fabric/core/Lifecycle.hh"
#include "fabric/utils/ErrorHandling.hh"
#include "fabric/utils/Testing.hh"
#include <gtest/gtest.h>
#include <memory>
#include <string>

using namespace fabric;
using namespace fabric::Testing;

class LifecycleTest : public ::testing::Test {
  protected:
    void SetUp() override {
        manager = std::make_unique<LifecycleManager>();
        recorder = std::make_unique<LifecycleRecorder>();
    }

    std::unique_ptr<LifecycleManager> manager;
    std::unique_ptr<LifecycleRecorder> recorder;
};

TEST_F(LifecycleTest, InitialState) {
    EXPECT_EQ(manager->getState(), LifecycleState::Created);
}

TEST_F(LifecycleTest, ValidStateTransition) {
    EXPECT_EQ(manager->getState(), LifecycleState::Created);

    manager->setState(LifecycleState::Initialized);
    EXPECT_EQ(manager->getState(), LifecycleState::Initialized);

    manager->setState(LifecycleState::Rendered);
    EXPECT_EQ(manager->getState(), LifecycleState::Rendered);

    manager->setState(LifecycleState::Updating);
    EXPECT_EQ(manager->getState(), LifecycleState::Updating);

    manager->setState(LifecycleState::Rendered);
    EXPECT_EQ(manager->getState(), LifecycleState::Rendered);

    manager->setState(LifecycleState::Suspended);
    EXPECT_EQ(manager->getState(), LifecycleState::Suspended);

    manager->setState(LifecycleState::Rendered);
    EXPECT_EQ(manager->getState(), LifecycleState::Rendered);

    manager->setState(LifecycleState::Destroyed);
    EXPECT_EQ(manager->getState(), LifecycleState::Destroyed);
}

TEST_F(LifecycleTest, InvalidStateTransition) {
    // Created -> Rendered (invalid, must go through Initialized)
    EXPECT_THROW(manager->setState(LifecycleState::Rendered), FabricException);

    // Created -> Updating (invalid)
    EXPECT_THROW(manager->setState(LifecycleState::Updating), FabricException);

    // Set to valid state first
    manager->setState(LifecycleState::Initialized);
    manager->setState(LifecycleState::Rendered);
    manager->setState(LifecycleState::Destroyed);

    // Destroyed -> any state (invalid, Destroyed is terminal)
    EXPECT_THROW(manager->setState(LifecycleState::Created), FabricException);
    EXPECT_THROW(manager->setState(LifecycleState::Initialized), FabricException);
    EXPECT_THROW(manager->setState(LifecycleState::Rendered), FabricException);
    EXPECT_THROW(manager->setState(LifecycleState::Updating), FabricException);
    EXPECT_THROW(manager->setState(LifecycleState::Suspended), FabricException);
}

TEST_F(LifecycleTest, IsValidTransition) {
    // Valid transitions
    EXPECT_TRUE(LifecycleManager::isValidTransition(LifecycleState::Created, LifecycleState::Created));
    EXPECT_TRUE(LifecycleManager::isValidTransition(LifecycleState::Created, LifecycleState::Initialized));
    EXPECT_TRUE(LifecycleManager::isValidTransition(LifecycleState::Created, LifecycleState::Destroyed));
    EXPECT_TRUE(LifecycleManager::isValidTransition(LifecycleState::Initialized, LifecycleState::Rendered));
    EXPECT_TRUE(LifecycleManager::isValidTransition(LifecycleState::Rendered, LifecycleState::Updating));
    EXPECT_TRUE(LifecycleManager::isValidTransition(LifecycleState::Updating, LifecycleState::Rendered));
    EXPECT_TRUE(LifecycleManager::isValidTransition(LifecycleState::Rendered, LifecycleState::Suspended));
    EXPECT_TRUE(LifecycleManager::isValidTransition(LifecycleState::Suspended, LifecycleState::Initialized));
    EXPECT_TRUE(LifecycleManager::isValidTransition(LifecycleState::Suspended, LifecycleState::Rendered));

    // Invalid transitions
    EXPECT_FALSE(LifecycleManager::isValidTransition(LifecycleState::Created, LifecycleState::Rendered));
    EXPECT_FALSE(LifecycleManager::isValidTransition(LifecycleState::Created, LifecycleState::Updating));
    EXPECT_FALSE(LifecycleManager::isValidTransition(LifecycleState::Created, LifecycleState::Suspended));
    EXPECT_FALSE(LifecycleManager::isValidTransition(LifecycleState::Destroyed, LifecycleState::Created));
    EXPECT_FALSE(LifecycleManager::isValidTransition(LifecycleState::Destroyed, LifecycleState::Initialized));
    EXPECT_FALSE(LifecycleManager::isValidTransition(LifecycleState::Destroyed, LifecycleState::Rendered));
}

TEST_F(LifecycleTest, StateToString) {
    EXPECT_EQ(lifecycleStateToString(LifecycleState::Created), "Created");
    EXPECT_EQ(lifecycleStateToString(LifecycleState::Initialized), "Initialized");
    EXPECT_EQ(lifecycleStateToString(LifecycleState::Rendered), "Rendered");
    EXPECT_EQ(lifecycleStateToString(LifecycleState::Updating), "Updating");
    EXPECT_EQ(lifecycleStateToString(LifecycleState::Suspended), "Suspended");
    EXPECT_EQ(lifecycleStateToString(LifecycleState::Destroyed), "Destroyed");
    EXPECT_EQ(lifecycleStateToString(static_cast<LifecycleState>(99)), "Unknown");
}

TEST_F(LifecycleTest, AddHook) {
    std::string hookId = manager->addHook(LifecycleState::Initialized, recorder->getHook());
    EXPECT_FALSE(hookId.empty());

    // Trigger the hook
    manager->setState(LifecycleState::Initialized);
    EXPECT_EQ(recorder->stateChanges, 1);

    // Hook shouldn't be called on other state changes
    manager->setState(LifecycleState::Rendered);
    EXPECT_EQ(recorder->stateChanges, 1);
}

TEST_F(LifecycleTest, AddHookThrowsOnNullHook) {
    EXPECT_THROW(manager->addHook(LifecycleState::Initialized, nullptr), FabricException);
}

TEST_F(LifecycleTest, AddTransitionHook) {
    std::string hookId =
        manager->addTransitionHook(LifecycleState::Initialized, LifecycleState::Rendered,
                                   recorder->getTransitionHook(LifecycleState::Initialized, LifecycleState::Rendered));
    EXPECT_FALSE(hookId.empty());

    // Trigger the hook
    manager->setState(LifecycleState::Initialized);
    EXPECT_EQ(recorder->transitionChanges, 0);

    manager->setState(LifecycleState::Rendered);
    EXPECT_EQ(recorder->transitionChanges, 1);
    EXPECT_EQ(recorder->lastFromState, LifecycleState::Initialized);
    EXPECT_EQ(recorder->lastToState, LifecycleState::Rendered);

    // Hook shouldn't be called on other transitions
    manager->setState(LifecycleState::Updating);
    EXPECT_EQ(recorder->transitionChanges, 1);
}

TEST_F(LifecycleTest, AddTransitionHookThrowsOnNullHook) {
    EXPECT_THROW(manager->addTransitionHook(LifecycleState::Initialized, LifecycleState::Rendered, nullptr),
                 FabricException);
}

TEST_F(LifecycleTest, RemoveHook) {
    std::string hookId = manager->addHook(LifecycleState::Initialized, recorder->getHook());
    EXPECT_TRUE(manager->removeHook(hookId));
    EXPECT_FALSE(manager->removeHook(hookId)); // Already removed
    EXPECT_FALSE(manager->removeHook("nonexistent"));

    // Hook should not be called after removal
    manager->setState(LifecycleState::Initialized);
    EXPECT_EQ(recorder->stateChanges, 0);
}

TEST_F(LifecycleTest, RemoveTransitionHook) {
    std::string hookId =
        manager->addTransitionHook(LifecycleState::Initialized, LifecycleState::Rendered,
                                   recorder->getTransitionHook(LifecycleState::Initialized, LifecycleState::Rendered));
    EXPECT_TRUE(manager->removeHook(hookId));
    EXPECT_FALSE(manager->removeHook(hookId)); // Already removed

    // Hook should not be called after removal
    manager->setState(LifecycleState::Initialized);
    manager->setState(LifecycleState::Rendered);
    EXPECT_EQ(recorder->transitionChanges, 0);
}

TEST_F(LifecycleTest, MultipleHooks) {
    int hook1Calls = 0;
    int hook2Calls = 0;

    manager->addHook(LifecycleState::Initialized, [&hook1Calls]() { hook1Calls++; });

    manager->addHook(LifecycleState::Initialized, [&hook2Calls]() { hook2Calls++; });

    manager->setState(LifecycleState::Initialized);
    EXPECT_EQ(hook1Calls, 1);
    EXPECT_EQ(hook2Calls, 1);
}
