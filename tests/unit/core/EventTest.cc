#include "fabric/core/Event.hh"
#include "fabric/utils/ErrorHandling.hh"
#include "fabric/utils/Testing.hh"
#include <cstdint>
#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <vector>

using namespace fabric;
using namespace fabric::Testing;

class EventTest : public ::testing::Test {
  protected:
    void SetUp() override {
        // Create test events
        testEvent1 = std::make_unique<Event>("click", "button1");
        testEvent2 = std::make_unique<Event>("input", "textfield1");
        dispatcher = std::make_unique<EventDispatcher>();
        recorder = std::make_unique<EventRecorder>();
    }

    std::unique_ptr<Event> testEvent1;
    std::unique_ptr<Event> testEvent2;
    std::unique_ptr<EventDispatcher> dispatcher;
    std::unique_ptr<EventRecorder> recorder;
};

TEST_F(EventTest, ConstructorThrowsOnEmptyType) {
    EXPECT_THROW(Event("", "source"), FabricException);
}

TEST_F(EventTest, GetType) {
    EXPECT_EQ(testEvent1->getType(), "click");
    EXPECT_EQ(testEvent2->getType(), "input");
}

TEST_F(EventTest, GetSource) {
    EXPECT_EQ(testEvent1->getSource(), "button1");
    EXPECT_EQ(testEvent2->getSource(), "textfield1");
}

TEST_F(EventTest, SetGetData) {
    testEvent1->setData<int>("intData", 42);
    testEvent1->setData<float>("floatData", 3.14f);
    testEvent1->setData<std::string>("stringData", "hello");
    testEvent1->setData<bool>("boolData", true);

    EXPECT_EQ(testEvent1->getData<int>("intData"), 42);
    EXPECT_FLOAT_EQ(testEvent1->getData<float>("floatData"), 3.14f);
    EXPECT_EQ(testEvent1->getData<std::string>("stringData"), "hello");
    EXPECT_EQ(testEvent1->getData<bool>("boolData"), true);
}

TEST_F(EventTest, GetDataThrowsOnMissingKey) {
    EXPECT_THROW(testEvent1->getData<int>("nonexistent"), FabricException);
}

TEST_F(EventTest, GetDataThrowsOnWrongType) {
    testEvent1->setData<int>("intData", 42);
    EXPECT_THROW(testEvent1->getData<std::string>("intData"), FabricException);
}

TEST_F(EventTest, HandledFlag) {
    EXPECT_FALSE(testEvent1->isHandled());

    testEvent1->setHandled(true);
    EXPECT_TRUE(testEvent1->isHandled());

    testEvent1->setHandled(false);
    EXPECT_FALSE(testEvent1->isHandled());
}

TEST_F(EventTest, AddEventListener) {
    std::string handlerId = dispatcher->addEventListener("click", recorder->getHandler());
    EXPECT_FALSE(handlerId.empty());
}

TEST_F(EventTest, AddEventListenerThrowsOnEmptyType) {
    EXPECT_THROW(dispatcher->addEventListener("", recorder->getHandler()), FabricException);
}

TEST_F(EventTest, AddEventListenerThrowsOnNullHandler) {
    EXPECT_THROW(dispatcher->addEventListener("click", nullptr), FabricException);
}

TEST_F(EventTest, RemoveEventListener) {
    std::string handlerId = dispatcher->addEventListener("click", recorder->getHandler());
    EXPECT_TRUE(dispatcher->removeEventListener("click", handlerId));
    EXPECT_FALSE(dispatcher->removeEventListener("click", handlerId)); // Already removed
    EXPECT_FALSE(dispatcher->removeEventListener("nonexistent", "invalid"));
}

TEST_F(EventTest, DispatchEvent) {
    // Create listener that doesn't mark the event as handled
    dispatcher->addEventListener("click", [this](const Event& event) { recorder->recordEvent(event); });

    EXPECT_FALSE(dispatcher->dispatchEvent(*testEvent1)); // Returns false because event not marked as handled
    EXPECT_EQ(recorder->eventCount, 1);
    EXPECT_EQ(recorder->lastEventType, "click");
    EXPECT_EQ(recorder->lastEventSource, "button1");

    EXPECT_FALSE(dispatcher->dispatchEvent(*testEvent2)); // No listeners for "input"
    EXPECT_EQ(recorder->eventCount, 1);                   // Should not have changed
}

TEST_F(EventTest, EventHandling) {
    dispatcher->addEventListener("click", [](Event& event) { event.setHandled(true); });

    EXPECT_TRUE(dispatcher->dispatchEvent(*testEvent1));
    EXPECT_TRUE(testEvent1->isHandled());
}

TEST_F(EventTest, MultipleEventListeners) {
    int handler1Calls = 0;
    int handler2Calls = 0;

    dispatcher->addEventListener("click", [&handler1Calls](const Event& event) { handler1Calls++; });

    dispatcher->addEventListener("click", [&handler2Calls](Event& event) {
        handler2Calls++;
        event.setHandled(true);
    });

    dispatcher->addEventListener("click", [](const Event& event) {
        // This should not be called because event is handled by previous listener
        FAIL() << "This handler should not be called";
    });

    EXPECT_TRUE(dispatcher->dispatchEvent(*testEvent1));
    EXPECT_EQ(handler1Calls, 1);
    EXPECT_EQ(handler2Calls, 1);
}

// Priority ordering tests

TEST_F(EventTest, PriorityOrdering) {
    std::vector<int> order;

    dispatcher->addEventListener("click", [&](Event&) { order.push_back(2); }, 10);
    dispatcher->addEventListener("click", [&](Event&) { order.push_back(0); }, -5);
    dispatcher->addEventListener("click", [&](Event&) { order.push_back(1); }, 0);

    dispatcher->dispatchEvent(*testEvent1);

    ASSERT_EQ(order.size(), 3u);
    EXPECT_EQ(order[0], 0); // priority -5 first
    EXPECT_EQ(order[1], 1); // priority 0 second
    EXPECT_EQ(order[2], 2); // priority 10 last
}

TEST_F(EventTest, SamePriorityPreservesInsertionOrder) {
    std::vector<int> order;

    dispatcher->addEventListener("click", [&](Event&) { order.push_back(0); });
    dispatcher->addEventListener("click", [&](Event&) { order.push_back(1); });
    dispatcher->addEventListener("click", [&](Event&) { order.push_back(2); });

    dispatcher->dispatchEvent(*testEvent1);

    ASSERT_EQ(order.size(), 3u);
    EXPECT_EQ(order[0], 0);
    EXPECT_EQ(order[1], 1);
    EXPECT_EQ(order[2], 2);
}

TEST_F(EventTest, HighPriorityHandlerStopsLower) {
    int lowCalls = 0;

    // High priority handler (runs first) marks handled
    dispatcher->addEventListener("click", [](Event& e) { e.setHandled(true); }, -10);
    // Default priority handler should not run
    dispatcher->addEventListener("click", [&](Event&) { lowCalls++; }, 0);

    EXPECT_TRUE(dispatcher->dispatchEvent(*testEvent1));
    EXPECT_EQ(lowCalls, 0);
}

// Cancellation tests

TEST_F(EventTest, CancelledFlag) {
    EXPECT_FALSE(testEvent1->isCancelled());

    testEvent1->setCancelled(true);
    EXPECT_TRUE(testEvent1->isCancelled());

    testEvent1->setCancelled(false);
    EXPECT_FALSE(testEvent1->isCancelled());
}

TEST_F(EventTest, CancellationStopsPropagation) {
    int calls = 0;

    dispatcher->addEventListener("click", [](Event& e) { e.setCancelled(true); });
    dispatcher->addEventListener("click", [&](Event&) { calls++; });

    EXPECT_TRUE(dispatcher->dispatchEvent(*testEvent1));
    EXPECT_TRUE(testEvent1->isCancelled());
    EXPECT_EQ(calls, 0);
}

// Any-typed data tests

TEST_F(EventTest, AnyDataSetGet) {
    testEvent1->setAnyData<std::vector<int>>("nums", {1, 2, 3});

    auto result = testEvent1->getAnyData<std::vector<int>>("nums");
    ASSERT_EQ(result.size(), 3u);
    EXPECT_EQ(result[0], 1);
    EXPECT_EQ(result[2], 3);
}

TEST_F(EventTest, AnyDataThrowsOnMissingKey) {
    EXPECT_THROW(testEvent1->getAnyData<int>("nope"), FabricException);
}

TEST_F(EventTest, AnyDataThrowsOnWrongType) {
    testEvent1->setAnyData<int>("val", 42);
    EXPECT_THROW(testEvent1->getAnyData<std::string>("val"), FabricException);
}

TEST_F(EventTest, HasAnyData) {
    EXPECT_FALSE(testEvent1->hasAnyData("key"));
    testEvent1->setAnyData<int>("key", 1);
    EXPECT_TRUE(testEvent1->hasAnyData("key"));
}

TEST_F(EventTest, AnyDataAndVariantDataCoexist) {
    testEvent1->setData<int>("variant_val", 10);
    testEvent1->setAnyData<double>("any_val", 3.14);

    EXPECT_EQ(testEvent1->getData<int>("variant_val"), 10);
    EXPECT_DOUBLE_EQ(testEvent1->getAnyData<double>("any_val"), 3.14);
    EXPECT_TRUE(testEvent1->hasData("variant_val"));
    EXPECT_FALSE(testEvent1->hasData("any_val")); // different storage
    EXPECT_TRUE(testEvent1->hasAnyData("any_val"));
}

// BinaryData in Variant test

TEST_F(EventTest, BinaryDataInVariant) {
    std::vector<uint8_t> payload = {0xDE, 0xAD, 0xBE, 0xEF};
    testEvent1->setData<std::vector<uint8_t>>("binary", payload);

    auto result = testEvent1->getData<std::vector<uint8_t>>("binary");
    ASSERT_EQ(result.size(), 4u);
    EXPECT_EQ(result[0], 0xDE);
    EXPECT_EQ(result[3], 0xEF);
}
