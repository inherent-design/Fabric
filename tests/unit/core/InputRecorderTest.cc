#include "fabric/core/InputRecorder.hh"
#include <gtest/gtest.h>

using namespace fabric;

// --- SerializedEvent construction ---

TEST(InputRecorderTest, SerializedEventDefaultConstruction) {
    SerializedEvent event;
    EXPECT_EQ(event.eventType, 0u);
    EXPECT_EQ(event.keycode, 0);
    EXPECT_EQ(event.mouseX, 0);
    EXPECT_EQ(event.mouseY, 0);
    EXPECT_EQ(event.mouseDeltaX, 0);
    EXPECT_EQ(event.mouseDeltaY, 0);
    EXPECT_EQ(event.button, 0);
    EXPECT_EQ(event.modifiers, 0);
    EXPECT_TRUE(event.text.empty());
}

TEST(InputRecorderTest, SerializedEventAllFields) {
    SerializedEvent event;
    event.eventType = static_cast<uint32_t>(InputEventType::KEY_DOWN);
    event.keycode = 119; // 'w'
    event.mouseX = 400;
    event.mouseY = 300;
    event.mouseDeltaX = -5;
    event.mouseDeltaY = 10;
    event.button = 1;
    event.modifiers = MOD_SHIFT | MOD_CTRL;
    event.text = "w";

    EXPECT_EQ(event.eventType, static_cast<uint32_t>(InputEventType::KEY_DOWN));
    EXPECT_EQ(event.keycode, 119);
    EXPECT_EQ(event.mouseX, 400);
    EXPECT_EQ(event.mouseY, 300);
    EXPECT_EQ(event.mouseDeltaX, -5);
    EXPECT_EQ(event.mouseDeltaY, 10);
    EXPECT_EQ(event.button, 1);
    EXPECT_EQ(event.modifiers, MOD_SHIFT | MOD_CTRL);
    EXPECT_EQ(event.text, "w");
}

// --- InputFrame construction ---

TEST(InputRecorderTest, InputFrameWithMultipleEvents) {
    InputFrame frame;
    frame.frameNumber = 42;
    frame.deltaTime = 0.016f;

    SerializedEvent keyDown;
    keyDown.eventType = static_cast<uint32_t>(InputEventType::KEY_DOWN);
    keyDown.keycode = 97; // 'a'
    keyDown.modifiers = MOD_NONE;

    SerializedEvent mouseMove;
    mouseMove.eventType = static_cast<uint32_t>(InputEventType::MOUSE_MOTION);
    mouseMove.mouseX = 200;
    mouseMove.mouseY = 150;
    mouseMove.mouseDeltaX = 3;
    mouseMove.mouseDeltaY = -2;

    frame.events.push_back(keyDown);
    frame.events.push_back(mouseMove);

    EXPECT_EQ(frame.frameNumber, 42u);
    EXPECT_FLOAT_EQ(frame.deltaTime, 0.016f);
    ASSERT_EQ(frame.events.size(), 2u);
    EXPECT_EQ(frame.events[0].keycode, 97);
    EXPECT_EQ(frame.events[1].mouseX, 200);
}

// --- InputRecording construction and metadata ---

TEST(InputRecorderTest, InputRecordingWithMetadata) {
    InputRecording recording;
    recording.metadata.version = "1.0";
    recording.metadata.description = "test recording";
    recording.metadata.totalFrames = 100;
    recording.metadata.totalDuration = 1.6f;

    EXPECT_EQ(recording.metadata.version, "1.0");
    EXPECT_EQ(recording.metadata.description, "test recording");
    EXPECT_EQ(recording.metadata.totalFrames, 100u);
    EXPECT_FLOAT_EQ(recording.metadata.totalDuration, 1.6f);
}

// --- JSON round-trip: SerializedEvent ---

TEST(InputRecorderTest, SerializedEventJsonRoundTrip) {
    SerializedEvent original;
    original.eventType = static_cast<uint32_t>(InputEventType::TEXT_INPUT);
    original.keycode = 0;
    original.mouseX = 512;
    original.mouseY = 384;
    original.mouseDeltaX = 0;
    original.mouseDeltaY = 0;
    original.button = 0;
    original.modifiers = MOD_ALT;
    original.text = "hello";

    nlohmann::json j = original;
    SerializedEvent restored = j.get<SerializedEvent>();

    EXPECT_EQ(original, restored);
}

// --- JSON round-trip: InputFrame ---

TEST(InputRecorderTest, InputFrameJsonRoundTrip) {
    InputFrame original;
    original.frameNumber = 999;
    original.deltaTime = 0.033f;

    SerializedEvent e1;
    e1.eventType = static_cast<uint32_t>(InputEventType::MOUSE_BUTTON_DOWN);
    e1.mouseX = 100;
    e1.mouseY = 200;
    e1.button = 3;
    e1.modifiers = MOD_GUI;

    SerializedEvent e2;
    e2.eventType = static_cast<uint32_t>(InputEventType::KEY_UP);
    e2.keycode = 27; // escape
    e2.modifiers = MOD_NONE;

    original.events.push_back(e1);
    original.events.push_back(e2);

    nlohmann::json j = original;
    InputFrame restored = j.get<InputFrame>();

    EXPECT_EQ(original, restored);
}

// --- JSON round-trip: InputRecording ---

TEST(InputRecorderTest, InputRecordingJsonRoundTrip) {
    InputRecording original;
    original.metadata.version = "1.0";
    original.metadata.description = "full round-trip test";
    original.metadata.totalFrames = 2;
    original.metadata.totalDuration = 0.032f;

    InputFrame frame1;
    frame1.frameNumber = 0;
    frame1.deltaTime = 0.016f;
    SerializedEvent ke;
    ke.eventType = static_cast<uint32_t>(InputEventType::KEY_DOWN);
    ke.keycode = 119;
    ke.modifiers = MOD_SHIFT;
    frame1.events.push_back(ke);

    InputFrame frame2;
    frame2.frameNumber = 1;
    frame2.deltaTime = 0.016f;
    SerializedEvent me;
    me.eventType = static_cast<uint32_t>(InputEventType::MOUSE_MOTION);
    me.mouseX = 640;
    me.mouseY = 480;
    me.mouseDeltaX = 10;
    me.mouseDeltaY = -5;
    frame2.events.push_back(me);

    original.frames.push_back(frame1);
    original.frames.push_back(frame2);

    nlohmann::json j = original;
    InputRecording restored = j.get<InputRecording>();

    EXPECT_EQ(original, restored);
}

// --- Round-trip preserves all fields ---

TEST(InputRecorderTest, RoundTripPreservesAllFields) {
    SerializedEvent event;
    event.eventType = static_cast<uint32_t>(InputEventType::MOUSE_WHEEL);
    event.keycode = -1;
    event.mouseX = -100;
    event.mouseY = 9999;
    event.mouseDeltaX = -32768;
    event.mouseDeltaY = 32767;
    event.button = 5;
    event.modifiers = MOD_SHIFT | MOD_CTRL | MOD_ALT | MOD_GUI;
    event.text = "special chars: !@#$%^&*()";

    nlohmann::json j = event;
    SerializedEvent restored = j.get<SerializedEvent>();

    EXPECT_EQ(restored.eventType, event.eventType);
    EXPECT_EQ(restored.keycode, event.keycode);
    EXPECT_EQ(restored.mouseX, event.mouseX);
    EXPECT_EQ(restored.mouseY, event.mouseY);
    EXPECT_EQ(restored.mouseDeltaX, event.mouseDeltaX);
    EXPECT_EQ(restored.mouseDeltaY, event.mouseDeltaY);
    EXPECT_EQ(restored.button, event.button);
    EXPECT_EQ(restored.modifiers, event.modifiers);
    EXPECT_EQ(restored.text, event.text);
}

// --- Empty recording ---

TEST(InputRecorderTest, EmptyRecordingJsonRoundTrip) {
    InputRecording original;

    nlohmann::json j = original;
    InputRecording restored = j.get<InputRecording>();

    EXPECT_EQ(original, restored);
    EXPECT_EQ(restored.frameCount(), 0u);
    EXPECT_FLOAT_EQ(restored.totalDuration(), 0.0f);
    EXPECT_TRUE(restored.frames.empty());
    EXPECT_EQ(restored.metadata.version, "1.0");
}

// --- totalDuration sums correctly ---

TEST(InputRecorderTest, TotalDurationSumsCorrectly) {
    InputRecording recording;

    InputFrame f1;
    f1.frameNumber = 0;
    f1.deltaTime = 0.016f;
    recording.addFrame(f1);

    InputFrame f2;
    f2.frameNumber = 1;
    f2.deltaTime = 0.033f;
    recording.addFrame(f2);

    InputFrame f3;
    f3.frameNumber = 2;
    f3.deltaTime = 0.017f;
    recording.addFrame(f3);

    EXPECT_NEAR(recording.totalDuration(), 0.066f, 1e-5f);
}

// --- frameCount returns correct count ---

TEST(InputRecorderTest, FrameCountReturnsCorrectCount) {
    InputRecording recording;
    EXPECT_EQ(recording.frameCount(), 0u);

    InputFrame f;
    f.frameNumber = 0;
    f.deltaTime = 0.016f;
    recording.addFrame(f);
    EXPECT_EQ(recording.frameCount(), 1u);

    f.frameNumber = 1;
    recording.addFrame(f);
    EXPECT_EQ(recording.frameCount(), 2u);
}

// --- clear resets to empty state ---

TEST(InputRecorderTest, ClearResetsToEmptyState) {
    InputRecording recording;
    recording.metadata.version = "1.0";
    recording.metadata.description = "will be cleared";
    recording.metadata.totalFrames = 50;
    recording.metadata.totalDuration = 2.5f;

    InputFrame f;
    f.frameNumber = 0;
    f.deltaTime = 0.016f;
    SerializedEvent e;
    e.eventType = static_cast<uint32_t>(InputEventType::KEY_DOWN);
    e.keycode = 65;
    f.events.push_back(e);
    recording.addFrame(f);

    ASSERT_EQ(recording.frameCount(), 1u);

    recording.clear();

    EXPECT_EQ(recording.frameCount(), 0u);
    EXPECT_FLOAT_EQ(recording.totalDuration(), 0.0f);
    EXPECT_TRUE(recording.frames.empty());
    EXPECT_EQ(recording.metadata.version, "1.0");
    EXPECT_TRUE(recording.metadata.description.empty());
    EXPECT_EQ(recording.metadata.totalFrames, 0u);
    EXPECT_FLOAT_EQ(recording.metadata.totalDuration, 0.0f);
}

// --- addFrame appends correctly ---

TEST(InputRecorderTest, AddFrameAppendsCorrectly) {
    InputRecording recording;

    InputFrame f1;
    f1.frameNumber = 0;
    f1.deltaTime = 0.016f;

    InputFrame f2;
    f2.frameNumber = 1;
    f2.deltaTime = 0.017f;

    recording.addFrame(f1);
    recording.addFrame(f2);

    ASSERT_EQ(recording.frameCount(), 2u);
    EXPECT_EQ(recording.frames[0].frameNumber, 0u);
    EXPECT_EQ(recording.frames[1].frameNumber, 1u);
    EXPECT_FLOAT_EQ(recording.frames[0].deltaTime, 0.016f);
    EXPECT_FLOAT_EQ(recording.frames[1].deltaTime, 0.017f);
}

// --- InputEventType enum values ---

TEST(InputRecorderTest, InputEventTypeValues) {
    EXPECT_EQ(static_cast<uint32_t>(InputEventType::KEY_DOWN), 0u);
    EXPECT_EQ(static_cast<uint32_t>(InputEventType::KEY_UP), 1u);
    EXPECT_EQ(static_cast<uint32_t>(InputEventType::MOUSE_MOTION), 2u);
    EXPECT_EQ(static_cast<uint32_t>(InputEventType::MOUSE_BUTTON_DOWN), 3u);
    EXPECT_EQ(static_cast<uint32_t>(InputEventType::MOUSE_BUTTON_UP), 4u);
    EXPECT_EQ(static_cast<uint32_t>(InputEventType::MOUSE_WHEEL), 5u);
    EXPECT_EQ(static_cast<uint32_t>(InputEventType::TEXT_INPUT), 6u);
}

// --- Modifier flags ---

TEST(InputRecorderTest, ModifierFlagCombinations) {
    uint16_t combined = MOD_SHIFT | MOD_CTRL | MOD_ALT | MOD_GUI;
    EXPECT_EQ(combined & MOD_SHIFT, MOD_SHIFT);
    EXPECT_EQ(combined & MOD_CTRL, MOD_CTRL);
    EXPECT_EQ(combined & MOD_ALT, MOD_ALT);
    EXPECT_EQ(combined & MOD_GUI, MOD_GUI);
    EXPECT_EQ(MOD_NONE, 0);
}

// --- Metadata defaults ---

TEST(InputRecorderTest, MetadataDefaults) {
    InputRecordingMetadata meta;
    EXPECT_EQ(meta.version, "1.0");
    EXPECT_TRUE(meta.description.empty());
    EXPECT_EQ(meta.totalFrames, 0u);
    EXPECT_FLOAT_EQ(meta.totalDuration, 0.0f);
}

// ============================================================
// InputRecorder state machine tests (INH-51)
// ============================================================

// --- Initial state ---

TEST(InputRecorderTest, InitialModeIsIdle) {
    InputRecorder recorder;
    EXPECT_EQ(recorder.mode(), RecorderMode::Idle);
    EXPECT_FALSE(recorder.isRecording());
    EXPECT_FALSE(recorder.isPlaying());
}

// --- Record / stop state transitions ---

TEST(InputRecorderTest, BeginRecordingSetsRecordingMode) {
    InputRecorder recorder;
    EXPECT_TRUE(recorder.beginRecording());
    EXPECT_EQ(recorder.mode(), RecorderMode::Recording);
    EXPECT_TRUE(recorder.isRecording());
    EXPECT_FALSE(recorder.isPlaying());
}

TEST(InputRecorderTest, StopRecordingReturnsToIdle) {
    InputRecorder recorder;
    recorder.beginRecording();
    recorder.stopRecording();
    EXPECT_EQ(recorder.mode(), RecorderMode::Idle);
    EXPECT_FALSE(recorder.isRecording());
}

TEST(InputRecorderTest, BeginRecordingClearsPrevious) {
    InputRecorder recorder;
    recorder.beginRecording();

    SerializedEvent e;
    e.eventType = static_cast<uint32_t>(InputEventType::KEY_DOWN);
    e.keycode = 65;
    recorder.captureEvent(e);
    recorder.advanceFrame(0.016f);
    recorder.stopRecording();

    EXPECT_EQ(recorder.recording().frameCount(), 1u);

    // Begin a new recording — previous data is cleared
    recorder.beginRecording();
    EXPECT_EQ(recorder.recording().frameCount(), 0u);
}

// --- Capture events during recording ---

TEST(InputRecorderTest, CaptureEventsDuringRecording) {
    InputRecorder recorder;
    recorder.beginRecording();

    SerializedEvent e1;
    e1.eventType = static_cast<uint32_t>(InputEventType::KEY_DOWN);
    e1.keycode = 119; // 'w'

    SerializedEvent e2;
    e2.eventType = static_cast<uint32_t>(InputEventType::MOUSE_MOTION);
    e2.mouseX = 100;
    e2.mouseY = 200;

    recorder.captureEvent(e1);
    recorder.captureEvent(e2);
    recorder.advanceFrame(0.016f);

    recorder.stopRecording();

    ASSERT_EQ(recorder.recording().frameCount(), 1u);
    ASSERT_EQ(recorder.recording().frames[0].events.size(), 2u);
    EXPECT_EQ(recorder.recording().frames[0].events[0].keycode, 119);
    EXPECT_EQ(recorder.recording().frames[0].events[1].mouseX, 100);
}

TEST(InputRecorderTest, CaptureEventIgnoredWhenNotRecording) {
    InputRecorder recorder;

    SerializedEvent e;
    e.eventType = static_cast<uint32_t>(InputEventType::KEY_DOWN);
    e.keycode = 65;
    recorder.captureEvent(e); // should be ignored

    EXPECT_EQ(recorder.recording().frameCount(), 0u);
}

// --- advanceFrame during recording ---

TEST(InputRecorderTest, AdvanceFrameCreatesMultipleFrames) {
    InputRecorder recorder;
    recorder.beginRecording();

    SerializedEvent e;
    e.eventType = static_cast<uint32_t>(InputEventType::KEY_DOWN);
    e.keycode = 65;

    // Frame 0
    recorder.captureEvent(e);
    recorder.advanceFrame(0.016f);

    // Frame 1
    e.keycode = 66;
    recorder.captureEvent(e);
    recorder.advanceFrame(0.017f);

    // Frame 2 (empty)
    recorder.advanceFrame(0.016f);

    recorder.stopRecording();

    ASSERT_EQ(recorder.recording().frameCount(), 3u);
    EXPECT_EQ(recorder.recording().frames[0].frameNumber, 0u);
    EXPECT_EQ(recorder.recording().frames[1].frameNumber, 1u);
    EXPECT_EQ(recorder.recording().frames[2].frameNumber, 2u);
    EXPECT_FLOAT_EQ(recorder.recording().frames[0].deltaTime, 0.016f);
    EXPECT_FLOAT_EQ(recorder.recording().frames[1].deltaTime, 0.017f);
}

// --- stopRecording finalizes pending frame with events ---

TEST(InputRecorderTest, StopRecordingFinalizesPendingFrame) {
    InputRecorder recorder;
    recorder.beginRecording();

    SerializedEvent e;
    e.eventType = static_cast<uint32_t>(InputEventType::KEY_DOWN);
    e.keycode = 65;
    recorder.captureEvent(e);

    // Stop without calling advanceFrame — pending events should be saved
    recorder.stopRecording();

    ASSERT_EQ(recorder.recording().frameCount(), 1u);
    EXPECT_EQ(recorder.recording().frames[0].events.size(), 1u);
}

// --- Playback returns events in correct frame order ---

TEST(InputRecorderTest, PlaybackReturnsEventsInOrder) {
    InputRecorder recorder;
    recorder.beginRecording();

    SerializedEvent e1;
    e1.eventType = static_cast<uint32_t>(InputEventType::KEY_DOWN);
    e1.keycode = 65;
    recorder.captureEvent(e1);
    recorder.advanceFrame(0.016f);

    SerializedEvent e2;
    e2.eventType = static_cast<uint32_t>(InputEventType::KEY_UP);
    e2.keycode = 65;
    recorder.captureEvent(e2);
    recorder.advanceFrame(0.016f);

    recorder.stopRecording();
    ASSERT_EQ(recorder.recording().frameCount(), 2u);

    EXPECT_TRUE(recorder.startPlayback());
    EXPECT_TRUE(recorder.isPlaying());

    auto frame0 = recorder.getNextFrame();
    ASSERT_EQ(frame0.size(), 1u);
    EXPECT_EQ(frame0[0].keycode, 65);
    EXPECT_EQ(frame0[0].eventType, static_cast<uint32_t>(InputEventType::KEY_DOWN));

    auto frame1 = recorder.getNextFrame();
    ASSERT_EQ(frame1.size(), 1u);
    EXPECT_EQ(frame1[0].eventType, static_cast<uint32_t>(InputEventType::KEY_UP));
}

// --- Playback ends when frames exhausted ---

TEST(InputRecorderTest, PlaybackEndsWhenFramesExhausted) {
    InputRecorder recorder;
    recorder.beginRecording();

    SerializedEvent e;
    e.eventType = static_cast<uint32_t>(InputEventType::KEY_DOWN);
    e.keycode = 65;
    recorder.captureEvent(e);
    recorder.advanceFrame(0.016f);
    recorder.stopRecording();

    recorder.startPlayback();

    auto frame0 = recorder.getNextFrame();
    EXPECT_EQ(frame0.size(), 1u);

    // After last frame, mode returns to Idle
    EXPECT_EQ(recorder.mode(), RecorderMode::Idle);

    // Further calls return empty
    auto empty = recorder.getNextFrame();
    EXPECT_TRUE(empty.empty());
}

// --- Cannot record while playing ---

TEST(InputRecorderTest, CannotRecordWhilePlaying) {
    InputRecorder recorder;
    recorder.beginRecording();

    SerializedEvent e;
    e.eventType = static_cast<uint32_t>(InputEventType::KEY_DOWN);
    e.keycode = 65;
    recorder.captureEvent(e);
    recorder.advanceFrame(0.016f);
    recorder.stopRecording();

    recorder.startPlayback();
    EXPECT_TRUE(recorder.isPlaying());

    // Attempting to begin recording while playing should fail
    EXPECT_FALSE(recorder.beginRecording());
    EXPECT_TRUE(recorder.isPlaying()); // still playing
}

// --- Cannot play while recording ---

TEST(InputRecorderTest, CannotPlayWhileRecording) {
    InputRecorder recorder;
    recorder.beginRecording();

    SerializedEvent e;
    e.eventType = static_cast<uint32_t>(InputEventType::KEY_DOWN);
    e.keycode = 65;
    recorder.captureEvent(e);
    recorder.advanceFrame(0.016f);
    // Don't stop recording

    EXPECT_FALSE(recorder.startPlayback());
    EXPECT_TRUE(recorder.isRecording()); // still recording
}

// --- Cannot play empty recording ---

TEST(InputRecorderTest, CannotPlayEmptyRecording) {
    InputRecorder recorder;
    EXPECT_FALSE(recorder.startPlayback());
    EXPECT_EQ(recorder.mode(), RecorderMode::Idle);
}

// --- Save -> load -> playback roundtrip ---

TEST(InputRecorderTest, SaveLoadPlaybackRoundtrip) {
    // Record some events
    InputRecorder recorder;
    recorder.beginRecording();

    SerializedEvent e1;
    e1.eventType = static_cast<uint32_t>(InputEventType::KEY_DOWN);
    e1.keycode = 87; // 'W'
    e1.modifiers = MOD_SHIFT;
    recorder.captureEvent(e1);
    recorder.advanceFrame(0.016f);

    SerializedEvent e2;
    e2.eventType = static_cast<uint32_t>(InputEventType::MOUSE_MOTION);
    e2.mouseX = 320;
    e2.mouseY = 240;
    e2.mouseDeltaX = 5;
    e2.mouseDeltaY = -3;
    recorder.captureEvent(e2);
    recorder.advanceFrame(0.033f);

    recorder.stopRecording();

    // Serialize to JSON and back
    nlohmann::json j = recorder.recording();
    InputRecording loaded = j.get<InputRecording>();

    // Load into a fresh recorder
    InputRecorder recorder2;
    recorder2.setRecording(std::move(loaded));

    EXPECT_TRUE(recorder2.startPlayback());

    auto f0 = recorder2.getNextFrame();
    ASSERT_EQ(f0.size(), 1u);
    EXPECT_EQ(f0[0].keycode, 87);
    EXPECT_EQ(f0[0].modifiers, MOD_SHIFT);

    auto f1 = recorder2.getNextFrame();
    ASSERT_EQ(f1.size(), 1u);
    EXPECT_EQ(f1[0].mouseX, 320);
    EXPECT_EQ(f1[0].mouseDeltaY, -3);

    EXPECT_EQ(recorder2.mode(), RecorderMode::Idle);
}

// --- setRecording only works when Idle ---

TEST(InputRecorderTest, SetRecordingOnlyWorksWhenIdle) {
    InputRecorder recorder;
    recorder.beginRecording();

    InputRecording replacement;
    InputFrame f;
    f.frameNumber = 0;
    f.deltaTime = 0.016f;
    SerializedEvent e;
    e.eventType = static_cast<uint32_t>(InputEventType::KEY_DOWN);
    e.keycode = 99;
    f.events.push_back(e);
    replacement.addFrame(f);

    recorder.setRecording(std::move(replacement));
    // Should have been ignored because we're recording
    EXPECT_EQ(recorder.recording().frameCount(), 0u);
}

// --- Metadata updated on stopRecording ---

TEST(InputRecorderTest, MetadataUpdatedOnStopRecording) {
    InputRecorder recorder;
    recorder.beginRecording();

    SerializedEvent e;
    e.eventType = static_cast<uint32_t>(InputEventType::KEY_DOWN);
    e.keycode = 65;
    recorder.captureEvent(e);
    recorder.advanceFrame(0.016f);

    recorder.captureEvent(e);
    recorder.advanceFrame(0.033f);

    recorder.stopRecording();

    EXPECT_EQ(recorder.recording().metadata.totalFrames, 2u);
    EXPECT_NEAR(recorder.recording().metadata.totalDuration, 0.049f, 1e-5f);
}
