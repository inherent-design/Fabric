#include "fabric/core/Component.hh"
#include "fabric/core/Event.hh"
#include "fabric/core/Lifecycle.hh"
#include "fabric/core/Plugin.hh"
#include "fabric/utils/ErrorHandling.hh"
#include "fabric/utils/Testing.hh"
#include <gtest/gtest.h>
#include <memory>
#include <string>

using namespace fabric;
using namespace fabric::Testing;

/**
 * @brief Integration test suite for Core API components
 * 
 * This test suite verifies that the core components (Component, Event, Lifecycle, Plugin)
 * can interact correctly with each other in various scenarios.
 */
class CoreApiTest : public ::testing::Test {
protected:
  void SetUp() override {
    // Initialize test objects
    component = std::make_shared<MockComponent>("test-component");
    eventDispatcher = std::make_unique<EventDispatcher>();
    lifecycleManager = std::make_unique<LifecycleManager>();
    eventRecorder = std::make_unique<EventRecorder>();
    lifecycleRecorder = std::make_unique<LifecycleRecorder>();
  }

  std::shared_ptr<MockComponent> component;
  std::unique_ptr<EventDispatcher> eventDispatcher;
  std::unique_ptr<LifecycleManager> lifecycleManager;
  std::unique_ptr<EventRecorder> eventRecorder;
  std::unique_ptr<LifecycleRecorder> lifecycleRecorder;
};

TEST_F(CoreApiTest, ComponentLifecycleWithEvents) {
  // Register for lifecycle state changes
  lifecycleManager->addHook(LifecycleState::Initialized, [this]() {
    // Send initialization complete event
    Event initEvent("component-initialized", component->getId());
    eventDispatcher->dispatchEvent(initEvent);
  });
  
  // Register event listener
  eventDispatcher->addEventListener("component-initialized", eventRecorder->getHandler());
  
  // Trigger the lifecycle change
  lifecycleManager->setState(LifecycleState::Initialized);
  
  // Verify the event was dispatched
  EXPECT_EQ(eventRecorder->eventCount, 1);
  EXPECT_EQ(eventRecorder->lastEventType, "component-initialized");
  EXPECT_EQ(eventRecorder->lastEventSource, "test-component");
}

TEST_F(CoreApiTest, EventsTriggeringLifecycleChanges) {
  // Register event listener to trigger lifecycle change
  eventDispatcher->addEventListener("render-request", [this](const Event& event) {
    // Handle event by changing lifecycle state
    lifecycleManager->setState(LifecycleState::Rendered);
  });
  
  // Register lifecycle hook to detect change
  lifecycleManager->addHook(LifecycleState::Rendered, lifecycleRecorder->getHook());
  
  // Initial state should be Created
  EXPECT_EQ(lifecycleManager->getState(), LifecycleState::Created);
  
  // First set to Initialized (since we can't go directly to Rendered)
  lifecycleManager->setState(LifecycleState::Initialized);
  
  // Dispatch event
  Event renderEvent("render-request", "system");
  eventDispatcher->dispatchEvent(renderEvent);
  
  // Verify lifecycle state changed
  EXPECT_EQ(lifecycleManager->getState(), LifecycleState::Rendered);
  EXPECT_EQ(lifecycleRecorder->stateChanges, 1);
}

TEST_F(CoreApiTest, ComponentTreeWithEventsAndLifecycle) {
  // Create a component tree
  auto child1 = std::make_shared<MockComponent>("child1");
  auto child2 = std::make_shared<MockComponent>("child2");
  component->addChild(child1);
  component->addChild(child2);
  
  // Set up event handling
  eventDispatcher->addEventListener("parent-update", [this, child1, child2](const Event& event) {
    // When parent updates, send events to children
    Event childEvent("child-update", component->getId());
    childEvent.setData<std::string>("source", "parent");
    
    // Propagate to children - in a real app, this would iterate through the children
    Event child1Event("child-update", "child1");
    Event child2Event("child-update", "child2");
    
    eventDispatcher->dispatchEvent(child1Event);
    eventDispatcher->dispatchEvent(child2Event);
  });
  
  // Count child update events
  int childUpdates = 0;
  eventDispatcher->addEventListener("child-update", [&childUpdates](const Event& event) {
    childUpdates++;
  });
  
  // Trigger parent event
  Event parentEvent("parent-update", component->getId());
  eventDispatcher->dispatchEvent(parentEvent);
  
  // Verify children received updates
  EXPECT_EQ(childUpdates, 2);
}

TEST_F(CoreApiTest, ComponentPropertyBinding) {
  // Set up listener for property change events
  eventDispatcher->addEventListener("property-changed", eventRecorder->getHandler());
  
  // Create event-triggered property change
  component->setProperty<int>("counter", 0);
  
  // Function to increment counter and emit event
  auto incrementCounter = [this]() {
    int currentValue = component->getProperty<int>("counter");
    component->setProperty<int>("counter", currentValue + 1);
    
    Event event("property-changed", component->getId());
    event.setData<std::string>("property", "counter");
    event.setData<int>("value", currentValue + 1);
    eventDispatcher->dispatchEvent(event);
  };
  
  // Trigger multiple increments
  incrementCounter();
  incrementCounter();
  incrementCounter();
  
  // Verify property was updated
  EXPECT_EQ(component->getProperty<int>("counter"), 3);
  
  // Verify events were dispatched
  EXPECT_EQ(eventRecorder->eventCount, 3);
  EXPECT_EQ(eventRecorder->lastEventType, "property-changed");
}

