#include "fabric/core/InputManager.hh"
#include <gtest/gtest.h>
#include <string>
#include <vector>

using namespace fabric;

class InputManagerTest : public ::testing::Test {
protected:
    EventDispatcher dispatcher;
    InputManager input{dispatcher};

    // Helper: create a key down event
    static SDL_Event makeKeyDown(SDL_Keycode key) {
        SDL_Event e = {};
        e.type = SDL_EVENT_KEY_DOWN;
        e.key.key = key;
        e.key.down = true;
        e.key.repeat = false;
        return e;
    }

    // Helper: create a key up event
    static SDL_Event makeKeyUp(SDL_Keycode key) {
        SDL_Event e = {};
        e.type = SDL_EVENT_KEY_UP;
        e.key.key = key;
        e.key.down = false;
        e.key.repeat = false;
        return e;
    }

    // Helper: create a mouse motion event
    static SDL_Event makeMouseMotion(float x, float y, float xrel, float yrel) {
        SDL_Event e = {};
        e.type = SDL_EVENT_MOUSE_MOTION;
        e.motion.x = x;
        e.motion.y = y;
        e.motion.xrel = xrel;
        e.motion.yrel = yrel;
        return e;
    }

    // Helper: create a mouse button event
    static SDL_Event makeMouseButton(Uint8 button, bool down) {
        SDL_Event e = {};
        e.type = down ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
        e.button.button = button;
        e.button.down = down;
        return e;
    }
};

// Key binding and dispatch

TEST_F(InputManagerTest, UnboundKeyIsNotConsumed) {
    auto e = makeKeyDown(SDLK_W);
    EXPECT_FALSE(input.processEvent(e));
}

TEST_F(InputManagerTest, BoundKeyDispatchesAction) {
    std::string dispatched;
    dispatcher.addEventListener("move_forward", [&](Event& ev) {
        dispatched = ev.getType();
    });

    input.bindKey("move_forward", SDLK_W);
    auto e = makeKeyDown(SDLK_W);
    EXPECT_TRUE(input.processEvent(e));
    EXPECT_EQ(dispatched, "move_forward");
}

TEST_F(InputManagerTest, KeyUpDispatchesReleasedAction) {
    std::string dispatched;
    dispatcher.addEventListener("move_forward:released", [&](Event& ev) {
        dispatched = ev.getType();
    });

    input.bindKey("move_forward", SDLK_W);

    auto down = makeKeyDown(SDLK_W);
    input.processEvent(down);

    auto up = makeKeyUp(SDLK_W);
    EXPECT_TRUE(input.processEvent(up));
    EXPECT_EQ(dispatched, "move_forward:released");
}

// Action active state

TEST_F(InputManagerTest, ActionActiveWhileKeyHeld) {
    input.bindKey("jump", SDLK_SPACE);

    EXPECT_FALSE(input.isActionActive("jump"));

    auto down = makeKeyDown(SDLK_SPACE);
    input.processEvent(down);
    EXPECT_TRUE(input.isActionActive("jump"));

    auto up = makeKeyUp(SDLK_SPACE);
    input.processEvent(up);
    EXPECT_FALSE(input.isActionActive("jump"));
}

// Key repeat is ignored

TEST_F(InputManagerTest, KeyRepeatIsIgnored) {
    int count = 0;
    dispatcher.addEventListener("fire", [&](Event&) { count++; });
    input.bindKey("fire", SDLK_F);

    auto e = makeKeyDown(SDLK_F);
    input.processEvent(e);
    EXPECT_EQ(count, 1);

    // Simulate repeat
    SDL_Event repeat = {};
    repeat.type = SDL_EVENT_KEY_DOWN;
    repeat.key.key = SDLK_F;
    repeat.key.down = true;
    repeat.key.repeat = true;
    EXPECT_FALSE(input.processEvent(repeat));
    EXPECT_EQ(count, 1);
}

// Mouse motion

TEST_F(InputManagerTest, MouseMotionUpdatesPosition) {
    auto e = makeMouseMotion(100.0f, 200.0f, 5.0f, -3.0f);
    EXPECT_TRUE(input.processEvent(e));

    EXPECT_FLOAT_EQ(input.mouseX(), 100.0f);
    EXPECT_FLOAT_EQ(input.mouseY(), 200.0f);
    EXPECT_FLOAT_EQ(input.mouseDeltaX(), 5.0f);
    EXPECT_FLOAT_EQ(input.mouseDeltaY(), -3.0f);
}

TEST_F(InputManagerTest, MouseDeltaAccumulates) {
    input.processEvent(makeMouseMotion(10.0f, 10.0f, 2.0f, 3.0f));
    input.processEvent(makeMouseMotion(15.0f, 15.0f, 4.0f, 1.0f));

    EXPECT_FLOAT_EQ(input.mouseDeltaX(), 6.0f);
    EXPECT_FLOAT_EQ(input.mouseDeltaY(), 4.0f);
    // Position should be the latest value
    EXPECT_FLOAT_EQ(input.mouseX(), 15.0f);
}

TEST_F(InputManagerTest, BeginFrameResetsDelta) {
    input.processEvent(makeMouseMotion(50.0f, 50.0f, 10.0f, 20.0f));
    input.beginFrame();

    EXPECT_FLOAT_EQ(input.mouseDeltaX(), 0.0f);
    EXPECT_FLOAT_EQ(input.mouseDeltaY(), 0.0f);
    // Position should persist
    EXPECT_FLOAT_EQ(input.mouseX(), 50.0f);
}

// Mouse buttons

TEST_F(InputManagerTest, MouseButtonTracking) {
    EXPECT_FALSE(input.mouseButton(1));

    input.processEvent(makeMouseButton(1, true));
    EXPECT_TRUE(input.mouseButton(1));

    input.processEvent(makeMouseButton(1, false));
    EXPECT_FALSE(input.mouseButton(1));
}

TEST_F(InputManagerTest, MouseButtonOutOfRange) {
    // Button 0 or >5 should be safely ignored
    EXPECT_FALSE(input.mouseButton(0));
    EXPECT_FALSE(input.mouseButton(6));
}

// Multiple bindings

TEST_F(InputManagerTest, MultipleBindings) {
    std::vector<std::string> actions;
    dispatcher.addEventListener("left", [&](Event&) { actions.push_back("left"); });
    dispatcher.addEventListener("right", [&](Event&) { actions.push_back("right"); });

    input.bindKey("left", SDLK_A);
    input.bindKey("right", SDLK_D);

    input.processEvent(makeKeyDown(SDLK_A));
    input.processEvent(makeKeyDown(SDLK_D));

    ASSERT_EQ(actions.size(), 2u);
    EXPECT_EQ(actions[0], "left");
    EXPECT_EQ(actions[1], "right");
}

// Unbind

TEST_F(InputManagerTest, UnbindRemovesAction) {
    int count = 0;
    dispatcher.addEventListener("shoot", [&](Event&) { count++; });

    input.bindKey("shoot", SDLK_X);
    input.processEvent(makeKeyDown(SDLK_X));
    EXPECT_EQ(count, 1);

    input.unbindKey("shoot");
    // Need to release and re-press since key is still "down" internally
    auto up = makeKeyUp(SDLK_X);
    input.processEvent(up);

    auto down2 = makeKeyDown(SDLK_X);
    EXPECT_FALSE(input.processEvent(down2));
    EXPECT_EQ(count, 1);
}

// InputManager without dispatcher

TEST_F(InputManagerTest, WorksWithoutDispatcher) {
    InputManager noDispatcher;
    noDispatcher.bindKey("test", SDLK_T);

    auto e = makeKeyDown(SDLK_T);
    EXPECT_TRUE(noDispatcher.processEvent(e));
    EXPECT_TRUE(noDispatcher.isActionActive("test"));
}
