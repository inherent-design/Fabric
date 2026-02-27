#include "fabric/core/AnimationEvents.hh"

#include <gtest/gtest.h>

using namespace fabric;

TEST(AnimationEventsTest, InitShutdown) {
    AnimationEvents ae;
    ae.init();
    ae.shutdown();
}

TEST(AnimationEventsTest, RegisterClip) {
    AnimationEvents ae;
    ae.init();

    ClipId id = ae.registerClip("walk");
    EXPECT_NE(id, InvalidClipId);
    EXPECT_EQ(ae.clipCount(), 1u);
    EXPECT_EQ(ae.clipName(id), "walk");

    ae.shutdown();
}

TEST(AnimationEventsTest, AddMarker) {
    AnimationEvents ae;
    ae.init();

    ClipId id = ae.registerClip("run");
    ae.addMarker(id, {0.25f, AnimEventType::Footstep, "step.wav", 0.8f, ""});
    ae.addMarker(id, {0.75f, AnimEventType::Footstep, "step.wav", 0.8f, ""});
    EXPECT_EQ(ae.markerCount(id), 2u);

    ae.shutdown();
}

TEST(AnimationEventsTest, ProcessEventsSimple) {
    AnimationEvents ae;
    ae.init();

    ClipId id = ae.registerClip("attack");
    ae.addMarker(id, {0.5f, AnimEventType::Whoosh, "whoosh.wav", 1.0f, "swing"});

    auto events = ae.processEvents(id, 0.3f, 0.6f);
    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].type, AnimEventType::Whoosh);
    EXPECT_EQ(events[0].soundPath, "whoosh.wav");
    EXPECT_FLOAT_EQ(events[0].triggerTime, 0.5f);
    EXPECT_EQ(events[0].tag, "swing");

    ae.shutdown();
}

TEST(AnimationEventsTest, ProcessEventsNoFire) {
    AnimationEvents ae;
    ae.init();

    ClipId id = ae.registerClip("idle");
    ae.addMarker(id, {0.5f, AnimEventType::Custom, "", 1.0f, "blink"});

    auto events = ae.processEvents(id, 0.6f, 0.9f);
    EXPECT_TRUE(events.empty());

    ae.shutdown();
}

TEST(AnimationEventsTest, ProcessEventsMultiple) {
    AnimationEvents ae;
    ae.init();

    ClipId id = ae.registerClip("run");
    ae.addMarker(id, {0.2f, AnimEventType::Footstep, "left.wav", 1.0f, ""});
    ae.addMarker(id, {0.5f, AnimEventType::Footstep, "right.wav", 1.0f, ""});
    ae.addMarker(id, {0.8f, AnimEventType::Footstep, "left.wav", 1.0f, ""});

    auto events = ae.processEvents(id, 0.1f, 0.9f);
    ASSERT_EQ(events.size(), 3u);
    EXPECT_FLOAT_EQ(events[0].triggerTime, 0.2f);
    EXPECT_FLOAT_EQ(events[1].triggerTime, 0.5f);
    EXPECT_FLOAT_EQ(events[2].triggerTime, 0.8f);

    ae.shutdown();
}

TEST(AnimationEventsTest, ProcessEventsWrapAround) {
    AnimationEvents ae;
    ae.init();

    ClipId id = ae.registerClip("loop");
    ae.addMarker(id, {0.1f, AnimEventType::Impact, "land.wav", 1.0f, ""});
    ae.addMarker(id, {0.9f, AnimEventType::Footstep, "step.wav", 1.0f, ""});

    auto events = ae.processEvents(id, 0.8f, 0.2f);
    ASSERT_EQ(events.size(), 2u);
    EXPECT_FLOAT_EQ(events[0].triggerTime, 0.9f);
    EXPECT_FLOAT_EQ(events[1].triggerTime, 0.1f);

    ae.shutdown();
}

TEST(AnimationEventsTest, ProcessEventsExactBoundary) {
    AnimationEvents ae;
    ae.init();

    ClipId id = ae.registerClip("test");
    ae.addMarker(id, {0.5f, AnimEventType::Custom, "", 1.0f, "edge"});

    auto events = ae.processEvents(id, 0.5f, 0.7f);
    EXPECT_TRUE(events.empty());

    ae.shutdown();
}

TEST(AnimationEventsTest, CallbackFired) {
    AnimationEvents ae;
    ae.init();

    ClipId id = ae.registerClip("hit");
    ae.addMarker(id, {0.5f, AnimEventType::Impact, "hit.wav", 0.9f, "punch"});

    int callCount = 0;
    AnimEventData received;
    ae.setEventCallback([&](const AnimEventData& data) {
        callCount++;
        received = data;
    });

    ae.processEvents(id, 0.4f, 0.6f);
    EXPECT_EQ(callCount, 1);
    EXPECT_EQ(received.type, AnimEventType::Impact);
    EXPECT_EQ(received.soundPath, "hit.wav");
    EXPECT_FLOAT_EQ(received.volume, 0.9f);
    EXPECT_EQ(received.tag, "punch");

    ae.shutdown();
}

TEST(AnimationEventsTest, ClearMarkers) {
    AnimationEvents ae;
    ae.init();

    ClipId id = ae.registerClip("test");
    ae.addMarker(id, {0.3f, AnimEventType::Custom, "", 1.0f, ""});
    ae.addMarker(id, {0.7f, AnimEventType::Custom, "", 1.0f, ""});
    EXPECT_EQ(ae.markerCount(id), 2u);

    ae.clearMarkers(id);
    EXPECT_EQ(ae.markerCount(id), 0u);

    ae.shutdown();
}

TEST(AnimationEventsTest, RemoveClip) {
    AnimationEvents ae;
    ae.init();

    ClipId id1 = ae.registerClip("a");
    ae.registerClip("b");
    EXPECT_EQ(ae.clipCount(), 2u);

    ae.removeClip(id1);
    EXPECT_EQ(ae.clipCount(), 1u);

    ae.shutdown();
}

TEST(AnimationEventsTest, InvalidClipProcess) {
    AnimationEvents ae;
    ae.init();

    auto events = ae.processEvents(999, 0.0f, 1.0f);
    EXPECT_TRUE(events.empty());

    ae.shutdown();
}

TEST(AnimationEventsTest, MarkersSortedOnInsert) {
    AnimationEvents ae;
    ae.init();

    ClipId id = ae.registerClip("sort");
    ae.addMarker(id, {0.8f, AnimEventType::Custom, "", 1.0f, "c"});
    ae.addMarker(id, {0.2f, AnimEventType::Custom, "", 1.0f, "a"});
    ae.addMarker(id, {0.5f, AnimEventType::Custom, "", 1.0f, "b"});

    auto events = ae.processEvents(id, 0.0f, 1.0f);
    ASSERT_EQ(events.size(), 3u);
    EXPECT_EQ(events[0].tag, "a");
    EXPECT_EQ(events[1].tag, "b");
    EXPECT_EQ(events[2].tag, "c");

    ae.shutdown();
}

TEST(AnimationEventsTest, FootstepAndImpactTypes) {
    AnimationEvents ae;
    ae.init();

    ClipId id = ae.registerClip("combo");
    ae.addMarker(id, {0.25f, AnimEventType::Footstep, "step.wav", 1.0f, ""});
    ae.addMarker(id, {0.75f, AnimEventType::Impact, "hit.wav", 1.0f, ""});

    auto events = ae.processEvents(id, 0.0f, 1.0f);
    ASSERT_EQ(events.size(), 2u);
    EXPECT_EQ(events[0].type, AnimEventType::Footstep);
    EXPECT_EQ(events[1].type, AnimEventType::Impact);

    ae.shutdown();
}
