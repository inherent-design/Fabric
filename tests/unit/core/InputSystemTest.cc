#include "fabric/core/InputSystem.hh"
#include "fabric/core/Event.hh"
#include <gtest/gtest.h>

using namespace fabric;

class InputSystemTest : public ::testing::Test {
  protected:
    InputSystem input;

    // --- SDL event helpers ---

    static SDL_Event makeKeyDown(SDL_Keycode key) {
        SDL_Event e = {};
        e.type = SDL_EVENT_KEY_DOWN;
        e.key.key = key;
        e.key.down = true;
        e.key.repeat = false;
        return e;
    }

    static SDL_Event makeKeyUp(SDL_Keycode key) {
        SDL_Event e = {};
        e.type = SDL_EVENT_KEY_UP;
        e.key.key = key;
        e.key.down = false;
        e.key.repeat = false;
        return e;
    }

    static SDL_Event makeMouseMotion(float xrel, float yrel) {
        SDL_Event e = {};
        e.type = SDL_EVENT_MOUSE_MOTION;
        e.motion.x = 400;
        e.motion.y = 300;
        e.motion.xrel = xrel;
        e.motion.yrel = yrel;
        return e;
    }

    static SDL_Event makeMouseButton(uint8_t button, bool down) {
        SDL_Event e = {};
        e.type = down ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
        e.button.button = button;
        e.button.down = down;
        return e;
    }

    static SDL_Event makeGamepadButtonDown(SDL_GamepadButton button) {
        SDL_Event e = {};
        e.type = SDL_EVENT_GAMEPAD_BUTTON_DOWN;
        e.gbutton.button = button;
        e.gbutton.down = true;
        return e;
    }

    static SDL_Event makeGamepadButtonUp(SDL_GamepadButton button) {
        SDL_Event e = {};
        e.type = SDL_EVENT_GAMEPAD_BUTTON_UP;
        e.gbutton.button = button;
        e.gbutton.down = false;
        return e;
    }

    static SDL_Event makeGamepadAxis(SDL_GamepadAxis axis, int16_t value) {
        SDL_Event e = {};
        e.type = SDL_EVENT_GAMEPAD_AXIS_MOTION;
        e.gaxis.axis = axis;
        e.gaxis.value = value;
        return e;
    }

    static SDL_Event makeMouseWheel(float x, float y) {
        SDL_Event e = {};
        e.type = SDL_EVENT_MOUSE_WHEEL;
        e.wheel.x = x;
        e.wheel.y = y;
        return e;
    }

    // --- Context helpers ---

    std::shared_ptr<InputContext> makeGameplayContext() {
        auto ctx = std::make_shared<InputContext>("gameplay", 100);

        ActionBinding jump;
        jump.name = "jump";
        jump.sources.push_back(KeySource{SDLK_SPACE});
        jump.sources.push_back(GamepadButtonSource{SDL_GAMEPAD_BUTTON_SOUTH});
        ctx->addAction(jump);

        ActionBinding interact;
        interact.name = "interact";
        interact.sources.push_back(KeySource{SDLK_E});
        ctx->addAction(interact);

        AxisBinding moveX;
        moveX.name = "move_x";
        AxisSource kpSrc;
        kpSrc.useKeyPair = true;
        kpSrc.keyPair = {SDLK_A, SDLK_D};
        moveX.sources.push_back(kpSrc);
        ctx->addAxis(moveX);

        AxisBinding lookX;
        lookX.name = "look_x";
        AxisSource mouseSrc;
        mouseSrc.source = MouseAxisSource{InputAxisComponent::X};
        lookX.sources.push_back(mouseSrc);
        ctx->addAxis(lookX);

        return ctx;
    }

    std::shared_ptr<InputContext> makeGlobalContext() {
        auto ctx = std::make_shared<InputContext>("global", 0);
        ctx->setConsumeInput(false);

        ActionBinding quit;
        quit.name = "quit";
        quit.sources.push_back(KeySource{SDLK_ESCAPE});
        ctx->addAction(quit);

        return ctx;
    }
};

// --- Device state processing ---

TEST_F(InputSystemTest, KeyDownUpdatesDeviceState) {
    input.processEvent(makeKeyDown(SDLK_W));
    EXPECT_TRUE(input.deviceState().keysDown.count(SDLK_W));

    input.processEvent(makeKeyUp(SDLK_W));
    EXPECT_FALSE(input.deviceState().keysDown.count(SDLK_W));
}

TEST_F(InputSystemTest, KeyRepeatIgnored) {
    input.processEvent(makeKeyDown(SDLK_W));

    SDL_Event repeat = {};
    repeat.type = SDL_EVENT_KEY_DOWN;
    repeat.key.key = SDLK_W;
    repeat.key.down = true;
    repeat.key.repeat = true;
    input.processEvent(repeat);

    // Key should still be in set (no double-insert issue, just verifying)
    EXPECT_TRUE(input.deviceState().keysDown.count(SDLK_W));
}

TEST_F(InputSystemTest, MouseMotionUpdatesState) {
    input.processEvent(makeMouseMotion(5.0f, -3.0f));
    EXPECT_FLOAT_EQ(input.deviceState().mouseDeltaX, 5.0f);
    EXPECT_FLOAT_EQ(input.deviceState().mouseDeltaY, -3.0f);

    // Delta accumulates within a frame
    input.processEvent(makeMouseMotion(2.0f, 1.0f));
    EXPECT_FLOAT_EQ(input.deviceState().mouseDeltaX, 7.0f);
    EXPECT_FLOAT_EQ(input.deviceState().mouseDeltaY, -2.0f);
}

TEST_F(InputSystemTest, BeginFrameResetsDelta) {
    input.processEvent(makeMouseMotion(10.0f, 10.0f));
    input.processEvent(makeMouseWheel(0.0f, 3.0f));

    input.beginFrame();
    EXPECT_FLOAT_EQ(input.deviceState().mouseDeltaX, 0.0f);
    EXPECT_FLOAT_EQ(input.deviceState().mouseDeltaY, 0.0f);
    EXPECT_FLOAT_EQ(input.deviceState().scrollDeltaX, 0.0f);
    EXPECT_FLOAT_EQ(input.deviceState().scrollDeltaY, 0.0f);
}

TEST_F(InputSystemTest, GamepadButtonUpdatesState) {
    input.processEvent(makeGamepadButtonDown(SDL_GAMEPAD_BUTTON_SOUTH));
    EXPECT_TRUE(input.deviceState().gamepadButtons[SDL_GAMEPAD_BUTTON_SOUTH]);

    input.processEvent(makeGamepadButtonUp(SDL_GAMEPAD_BUTTON_SOUTH));
    EXPECT_FALSE(input.deviceState().gamepadButtons[SDL_GAMEPAD_BUTTON_SOUTH]);
}

TEST_F(InputSystemTest, GamepadAxisNormalized) {
    // Full positive
    input.processEvent(makeGamepadAxis(SDL_GAMEPAD_AXIS_LEFTX, 32767));
    EXPECT_NEAR(input.deviceState().gamepadAxes[SDL_GAMEPAD_AXIS_LEFTX], 1.0f, 0.001f);

    // Full negative
    input.processEvent(makeGamepadAxis(SDL_GAMEPAD_AXIS_LEFTX, -32768));
    EXPECT_NEAR(input.deviceState().gamepadAxes[SDL_GAMEPAD_AXIS_LEFTX], -1.0f, 0.01f);

    // Neutral
    input.processEvent(makeGamepadAxis(SDL_GAMEPAD_AXIS_LEFTX, 0));
    EXPECT_FLOAT_EQ(input.deviceState().gamepadAxes[SDL_GAMEPAD_AXIS_LEFTX], 0.0f);
}

// --- Action evaluation ---

TEST_F(InputSystemTest, ActionEvaluationKeyboard) {
    input.pushContext(makeGameplayContext());

    input.processEvent(makeKeyDown(SDLK_SPACE));
    input.evaluate();

    EXPECT_TRUE(input.isActionActive("jump"));
    EXPECT_TRUE(input.isActionJustPressed("jump"));
    EXPECT_FALSE(input.isActionActive("interact"));
}

TEST_F(InputSystemTest, ActionMultiSourceOR) {
    input.pushContext(makeGameplayContext());

    // Gamepad button also triggers "jump"
    input.processEvent(makeGamepadButtonDown(SDL_GAMEPAD_BUTTON_SOUTH));
    input.evaluate();

    EXPECT_TRUE(input.isActionActive("jump"));
}

TEST_F(InputSystemTest, ActionJustPressedToHeldTransition) {
    input.pushContext(makeGameplayContext());

    // Frame 1: press
    input.processEvent(makeKeyDown(SDLK_SPACE));
    input.evaluate();
    EXPECT_EQ(input.actionState("jump").state, ActionState::JustPressed);

    // Frame 2: still held
    input.beginFrame();
    input.evaluate();
    EXPECT_EQ(input.actionState("jump").state, ActionState::Held);

    // Frame 3: release
    input.beginFrame();
    input.processEvent(makeKeyUp(SDLK_SPACE));
    input.evaluate();
    EXPECT_EQ(input.actionState("jump").state, ActionState::JustReleased);

    // Frame 4: released
    input.beginFrame();
    input.evaluate();
    EXPECT_EQ(input.actionState("jump").state, ActionState::Released);
}

// --- Axis evaluation ---

TEST_F(InputSystemTest, AxisKeyPairEvaluation) {
    input.pushContext(makeGameplayContext());

    // Press D key (positive direction)
    input.processEvent(makeKeyDown(SDLK_D));
    input.evaluate();
    EXPECT_FLOAT_EQ(input.getAxisValue("move_x"), 1.0f);

    // Press A key (both held = cancel)
    input.processEvent(makeKeyDown(SDLK_A));
    input.evaluate();
    EXPECT_FLOAT_EQ(input.getAxisValue("move_x"), 0.0f);

    // Release D, only A held
    input.processEvent(makeKeyUp(SDLK_D));
    input.evaluate();
    EXPECT_FLOAT_EQ(input.getAxisValue("move_x"), -1.0f);
}

TEST_F(InputSystemTest, AxisMouseDelta) {
    input.pushContext(makeGameplayContext());

    input.processEvent(makeMouseMotion(15.0f, 0.0f));
    input.evaluate();

    // look_x uses mouse X delta, clamped to [-1, 1]
    EXPECT_FLOAT_EQ(input.getAxisValue("look_x"), 1.0f);
}

// --- Context stack ---

TEST_F(InputSystemTest, ContextPriorityOrdering) {
    auto global = makeGlobalContext();
    auto gameplay = makeGameplayContext();

    input.pushContext(gameplay);
    input.pushContext(global);

    // Should be sorted: gameplay (100) first, global (0) second
    ASSERT_EQ(input.contexts().size(), 2u);
    EXPECT_EQ(input.contexts()[0]->name(), "gameplay");
    EXPECT_EQ(input.contexts()[1]->name(), "global");
}

TEST_F(InputSystemTest, ContextConsumption) {
    // Higher-priority "menu" context consumes "quit" action
    auto menu = std::make_shared<InputContext>("menu", 200);
    menu->setConsumeInput(true);
    ActionBinding menuQuit;
    menuQuit.name = "quit";
    menuQuit.sources.push_back(KeySource{SDLK_ESCAPE});
    menu->addAction(menuQuit);

    auto global = makeGlobalContext(); // also has "quit" at priority 0

    input.pushContext(menu);
    input.pushContext(global);

    input.processEvent(makeKeyDown(SDLK_ESCAPE));
    input.evaluate();

    // "quit" action should be active (from menu context)
    EXPECT_TRUE(input.isActionActive("quit"));
    // The value comes from menu (priority 200), not global (priority 0)
    // Both define it, but menu consumes it
}

TEST_F(InputSystemTest, PopContext) {
    input.pushContext(makeGameplayContext());
    input.pushContext(makeGlobalContext());
    EXPECT_EQ(input.contexts().size(), 2u);

    EXPECT_TRUE(input.popContext("gameplay"));
    EXPECT_EQ(input.contexts().size(), 1u);
    EXPECT_EQ(input.contexts()[0]->name(), "global");

    EXPECT_FALSE(input.popContext("nonexistent"));
}

TEST_F(InputSystemTest, FindContext) {
    input.pushContext(makeGameplayContext());

    auto* found = input.findContext("gameplay");
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->name(), "gameplay");

    EXPECT_EQ(input.findContext("nonexistent"), nullptr);
}

TEST_F(InputSystemTest, DisabledContextSkipped) {
    auto ctx = makeGameplayContext();
    ctx->setEnabled(false);
    input.pushContext(ctx);

    input.processEvent(makeKeyDown(SDLK_SPACE));
    input.evaluate();

    // Context disabled, action should not be active
    EXPECT_FALSE(input.isActionActive("jump"));
}

// --- EventDispatcher integration ---

TEST_F(InputSystemTest, DispatcherFiresOnTransition) {
    EventDispatcher dispatcher;
    input.setDispatcher(&dispatcher);
    input.pushContext(makeGameplayContext());

    std::string pressedAction;
    std::string releasedAction;
    dispatcher.addEventListener("jump", [&](Event& ev) { pressedAction = ev.getType(); });
    dispatcher.addEventListener("jump:released", [&](Event& ev) { releasedAction = ev.getType(); });

    // Press
    input.processEvent(makeKeyDown(SDLK_SPACE));
    input.evaluate();
    EXPECT_EQ(pressedAction, "jump");

    // Release
    input.beginFrame();
    input.processEvent(makeKeyUp(SDLK_SPACE));
    input.evaluate();
    EXPECT_EQ(releasedAction, "jump:released");
}

// --- Unrecognized events ---

TEST_F(InputSystemTest, UnrecognizedEventReturnsFalse) {
    SDL_Event e = {};
    e.type = SDL_EVENT_QUIT;
    EXPECT_FALSE(input.processEvent(e));
}

// --- Empty state ---

TEST_F(InputSystemTest, QueryWithNoContextsReturnsDefaults) {
    EXPECT_FALSE(input.isActionActive("anything"));
    EXPECT_FLOAT_EQ(input.getAxisValue("anything"), 0.0f);
    EXPECT_EQ(input.actionState("anything").state, ActionState::Released);
}

// --- Audit: mouse button device state ---

TEST_F(InputSystemTest, MouseButtonUpdatesDeviceState) {
    input.processEvent(makeMouseButton(SDL_BUTTON_LEFT, true));
    EXPECT_TRUE(input.deviceState().mouseButtons[0]);

    input.processEvent(makeMouseButton(SDL_BUTTON_LEFT, false));
    EXPECT_FALSE(input.deviceState().mouseButtons[0]);

    input.processEvent(makeMouseButton(SDL_BUTTON_RIGHT, true));
    EXPECT_TRUE(input.deviceState().mouseButtons[2]);
}

TEST_F(InputSystemTest, MouseWheelUpdatesDeviceState) {
    input.processEvent(makeMouseWheel(0.0f, 3.0f));
    EXPECT_FLOAT_EQ(input.deviceState().scrollDeltaY, 3.0f);

    input.processEvent(makeMouseWheel(0.0f, -1.0f));
    EXPECT_FLOAT_EQ(input.deviceState().scrollDeltaY, 2.0f);
}

// --- Audit: out-of-range device inputs ---

TEST_F(InputSystemTest, MouseButtonOutOfRangeIgnored) {
    SDL_Event e = {};
    e.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
    e.button.button = 0;
    e.button.down = true;
    EXPECT_TRUE(input.processEvent(e));

    e.button.button = 6;
    EXPECT_TRUE(input.processEvent(e));

    for (int i = 0; i < 5; ++i) {
        EXPECT_FALSE(input.deviceState().mouseButtons[i]);
    }
}

// --- Audit: oneShot action lifecycle ---

TEST_F(InputSystemTest, OneShotActionLifecycle) {
    auto ctx = std::make_shared<InputContext>("test", 100);
    ActionBinding screenshot;
    screenshot.name = "screenshot";
    screenshot.sources.push_back(KeySource{SDLK_F12});
    screenshot.oneShot = true;
    ctx->addAction(screenshot);
    input.pushContext(ctx);

    // Frame 1: press
    input.processEvent(makeKeyDown(SDLK_F12));
    input.evaluate();
    EXPECT_EQ(input.actionState("screenshot").state, ActionState::JustPressed);

    // Frame 2: key still held, oneShot skips Held and goes to JustReleased
    input.beginFrame();
    input.evaluate();
    EXPECT_EQ(input.actionState("screenshot").state, ActionState::JustReleased);
}

// --- Audit: gamepad axis as digital action trigger ---

TEST_F(InputSystemTest, GamepadAxisTriggersDigitalAction) {
    auto ctx = std::make_shared<InputContext>("test", 100);
    ActionBinding accelerate;
    accelerate.name = "accelerate";
    accelerate.sources.push_back(GamepadAxisSource{SDL_GAMEPAD_AXIS_RIGHTX});
    ctx->addAction(accelerate);
    input.pushContext(ctx);

    // Below 0.5 threshold: not triggered
    input.processEvent(makeGamepadAxis(SDL_GAMEPAD_AXIS_RIGHTX, 10000));
    input.evaluate();
    EXPECT_FALSE(input.isActionActive("accelerate"));

    // Above 0.5 threshold: triggered
    input.beginFrame();
    input.processEvent(makeGamepadAxis(SDL_GAMEPAD_AXIS_RIGHTX, 20000));
    input.evaluate();
    EXPECT_TRUE(input.isActionActive("accelerate"));
}

// --- Audit: context stack consumption ---

TEST_F(InputSystemTest, NonConsumingContextPassesThrough) {
    auto gameplay = makeGameplayContext();
    auto global = makeGlobalContext();
    input.pushContext(gameplay);
    input.pushContext(global);

    input.processEvent(makeKeyDown(SDLK_ESCAPE));
    input.processEvent(makeKeyDown(SDLK_SPACE));
    input.evaluate();

    EXPECT_TRUE(input.isActionActive("quit"));
    EXPECT_TRUE(input.isActionActive("jump"));
}

TEST_F(InputSystemTest, ConsumingContextBlocksSameAction) {
    auto menu = std::make_shared<InputContext>("menu", 200);
    menu->setConsumeInput(true);
    ActionBinding menuQuit;
    menuQuit.name = "quit";
    menuQuit.sources.push_back(KeySource{SDLK_ESCAPE});
    menu->addAction(menuQuit);

    auto global = makeGlobalContext();
    input.pushContext(menu);
    input.pushContext(global);

    input.processEvent(makeKeyDown(SDLK_ESCAPE));
    input.evaluate();

    EXPECT_TRUE(input.isActionActive("quit"));
    EXPECT_TRUE(input.isActionJustPressed("quit"));
}

// --- Audit: clearContexts ---

TEST_F(InputSystemTest, ClearContextsRemovesAll) {
    input.pushContext(makeGameplayContext());
    input.pushContext(makeGlobalContext());
    EXPECT_EQ(input.contexts().size(), 2u);

    input.clearContexts();
    EXPECT_TRUE(input.contexts().empty());
}

// --- Audit: query after context removal ---

TEST_F(InputSystemTest, QueryAfterContextRemoval) {
    input.pushContext(makeGameplayContext());
    input.processEvent(makeKeyDown(SDLK_SPACE));
    input.evaluate();
    EXPECT_TRUE(input.isActionActive("jump"));

    input.popContext("gameplay");
    input.beginFrame();
    input.evaluate();
    EXPECT_FALSE(input.isActionActive("jump"));
    EXPECT_FLOAT_EQ(input.getAxisValue("move_x"), 0.0f);
}

// --- Audit: axis with gamepad source ---

TEST_F(InputSystemTest, AxisEvaluationWithGamepadSource) {
    auto ctx = std::make_shared<InputContext>("test", 100);
    AxisBinding lookX;
    lookX.name = "look_x";
    lookX.deadZone = 0.15f;
    AxisSource gpSrc;
    gpSrc.source = GamepadAxisSource{SDL_GAMEPAD_AXIS_RIGHTX};
    lookX.sources.push_back(gpSrc);
    ctx->addAxis(lookX);
    input.pushContext(ctx);

    input.processEvent(makeGamepadAxis(SDL_GAMEPAD_AXIS_RIGHTX, 16384));
    input.evaluate();

    float value = input.getAxisValue("look_x");
    EXPECT_GT(value, 0.0f);
    EXPECT_LE(value, 1.0f);
}

// --- Audit: evaluate with empty context stack ---

TEST_F(InputSystemTest, EvaluateWithEmptyContextStack) {
    input.processEvent(makeKeyDown(SDLK_SPACE));
    input.evaluate();

    EXPECT_FALSE(input.isActionActive("jump"));
    EXPECT_FLOAT_EQ(input.getAxisValue("anything"), 0.0f);
}

// --- Audit: multiple evaluate calls per frame ---

TEST_F(InputSystemTest, MultipleEvaluatesInSameFrame) {
    input.pushContext(makeGameplayContext());
    input.processEvent(makeKeyDown(SDLK_SPACE));

    input.evaluate();
    EXPECT_TRUE(input.isActionJustPressed("jump"));

    // Second evaluate: key still held, was active in prevActiveActions_
    input.evaluate();
    EXPECT_EQ(input.actionState("jump").state, ActionState::Held);
}

// --- Audit: disabled context does not consume ---

TEST_F(InputSystemTest, DisabledContextDoesNotConsumeInput) {
    auto menu = std::make_shared<InputContext>("menu", 200);
    menu->setConsumeInput(true);
    menu->setEnabled(false);
    ActionBinding menuAction;
    menuAction.name = "quit";
    menuAction.sources.push_back(KeySource{SDLK_ESCAPE});
    menu->addAction(menuAction);

    auto global = makeGlobalContext();
    input.pushContext(menu);
    input.pushContext(global);

    input.processEvent(makeKeyDown(SDLK_ESCAPE));
    input.evaluate();

    EXPECT_TRUE(input.isActionActive("quit"));
}
