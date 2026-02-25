#include "fabric/core/InputRouter.hh"
#include <gtest/gtest.h>
#include <string>
#include <vector>

using namespace fabric;

class InputRouterTest : public ::testing::Test {
  protected:
    EventDispatcher dispatcher;
    InputManager inputMgr{dispatcher};
    InputRouter router{inputMgr};

    static SDL_Event makeKeyDown(SDL_Keycode key, SDL_Keymod mod = SDL_KMOD_NONE,
                                 bool repeat = false) {
        SDL_Event e = {};
        e.type = SDL_EVENT_KEY_DOWN;
        e.key.key = key;
        e.key.mod = mod;
        e.key.down = true;
        e.key.repeat = repeat;
        return e;
    }

    static SDL_Event makeKeyUp(SDL_Keycode key, SDL_Keymod mod = SDL_KMOD_NONE) {
        SDL_Event e = {};
        e.type = SDL_EVENT_KEY_UP;
        e.key.key = key;
        e.key.mod = mod;
        e.key.down = false;
        e.key.repeat = false;
        return e;
    }

    static SDL_Event makeMouseMotion(float x, float y, float xrel, float yrel) {
        SDL_Event e = {};
        e.type = SDL_EVENT_MOUSE_MOTION;
        e.motion.x = x;
        e.motion.y = y;
        e.motion.xrel = xrel;
        e.motion.yrel = yrel;
        return e;
    }

    static SDL_Event makeMouseButton(Uint8 button, bool down) {
        SDL_Event e = {};
        e.type = down ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
        e.button.button = button;
        e.button.down = down;
        return e;
    }

    static SDL_Event makeMouseWheel(float y) {
        SDL_Event e = {};
        e.type = SDL_EVENT_MOUSE_WHEEL;
        e.wheel.y = y;
        return e;
    }

    // Cannot construct real SDL_EVENT_TEXT_INPUT (text is const char* managed by SDL),
    // so text input forwarding is tested at integration level.
};

// -- Default mode --

TEST_F(InputRouterTest, DefaultModeIsGameOnly) {
    EXPECT_EQ(router.mode(), InputMode::GameOnly);
}

// -- Mode setting --

TEST_F(InputRouterTest, SetModeChangesMode) {
    router.setMode(InputMode::UIOnly);
    EXPECT_EQ(router.mode(), InputMode::UIOnly);

    router.setMode(InputMode::GameAndUI);
    EXPECT_EQ(router.mode(), InputMode::GameAndUI);

    router.setMode(InputMode::GameOnly);
    EXPECT_EQ(router.mode(), InputMode::GameOnly);
}

// -- Escape toggle --

TEST_F(InputRouterTest, EscapeTogglesGameOnlyToUIOnly) {
    EXPECT_EQ(router.mode(), InputMode::GameOnly);

    auto esc = makeKeyDown(SDLK_ESCAPE);
    EXPECT_TRUE(router.routeEvent(esc, nullptr));
    EXPECT_EQ(router.mode(), InputMode::UIOnly);
}

TEST_F(InputRouterTest, EscapeTogglesUIOnlyToGameOnly) {
    router.setMode(InputMode::UIOnly);

    auto esc = makeKeyDown(SDLK_ESCAPE);
    EXPECT_TRUE(router.routeEvent(esc, nullptr));
    EXPECT_EQ(router.mode(), InputMode::GameOnly);
}

TEST_F(InputRouterTest, EscapeDoesNotToggleGameAndUI) {
    router.setMode(InputMode::GameAndUI);

    auto esc = makeKeyDown(SDLK_ESCAPE);
    router.routeEvent(esc, nullptr);
    // GameAndUI stays as-is (toggle only affects GameOnly<->UIOnly)
    EXPECT_EQ(router.mode(), InputMode::GameAndUI);
}

TEST_F(InputRouterTest, EscapeRepeatIsIgnoredForToggle) {
    auto escRepeat = makeKeyDown(SDLK_ESCAPE, SDL_KMOD_NONE, true);
    // Repeats should not toggle. They fall through to normal routing.
    router.routeEvent(escRepeat, nullptr);
    EXPECT_EQ(router.mode(), InputMode::GameOnly);
}

// -- GameOnly routing (null context) --

TEST_F(InputRouterTest, GameOnlyForwardsToInputManager) {
    inputMgr.bindKey("forward", SDLK_W);

    auto e = makeKeyDown(SDLK_W);
    EXPECT_TRUE(router.routeEvent(e, nullptr));
    EXPECT_TRUE(inputMgr.isActionActive("forward"));
}

TEST_F(InputRouterTest, GameOnlyForwardsMouseMotion) {
    auto e = makeMouseMotion(100.0f, 200.0f, 5.0f, -3.0f);
    EXPECT_TRUE(router.routeEvent(e, nullptr));

    EXPECT_FLOAT_EQ(inputMgr.mouseX(), 100.0f);
    EXPECT_FLOAT_EQ(inputMgr.mouseY(), 200.0f);
}

TEST_F(InputRouterTest, GameOnlyForwardsMouseButton) {
    auto e = makeMouseButton(1, true);
    EXPECT_TRUE(router.routeEvent(e, nullptr));
    EXPECT_TRUE(inputMgr.mouseButton(1));
}

// -- UIOnly with null context --

TEST_F(InputRouterTest, UIOnlyWithNullContextReturnsFalse) {
    router.setMode(InputMode::UIOnly);
    inputMgr.bindKey("forward", SDLK_W);

    auto e = makeKeyDown(SDLK_W);
    EXPECT_FALSE(router.routeEvent(e, nullptr));
    // InputManager should NOT receive the event
    EXPECT_FALSE(inputMgr.isActionActive("forward"));
}

// -- GameAndUI with null context falls back to game --

TEST_F(InputRouterTest, GameAndUIWithNullContextForwardsToGame) {
    router.setMode(InputMode::GameAndUI);
    inputMgr.bindKey("forward", SDLK_W);

    auto e = makeKeyDown(SDLK_W);
    EXPECT_TRUE(router.routeEvent(e, nullptr));
    EXPECT_TRUE(inputMgr.isActionActive("forward"));
}

// -- beginFrame delegates --

TEST_F(InputRouterTest, BeginFrameResetsInputManager) {
    auto motion = makeMouseMotion(50.0f, 50.0f, 10.0f, 20.0f);
    router.routeEvent(motion, nullptr);

    EXPECT_FLOAT_EQ(inputMgr.mouseDeltaX(), 10.0f);
    router.beginFrame();
    EXPECT_FLOAT_EQ(inputMgr.mouseDeltaX(), 0.0f);
}

// -- toggleUIMode --

TEST_F(InputRouterTest, ToggleFromGameOnly) {
    router.toggleUIMode();
    EXPECT_EQ(router.mode(), InputMode::UIOnly);
}

TEST_F(InputRouterTest, ToggleFromUIOnly) {
    router.setMode(InputMode::UIOnly);
    router.toggleUIMode();
    EXPECT_EQ(router.mode(), InputMode::GameOnly);
}

TEST_F(InputRouterTest, ToggleFromGameAndUIDoesNothing) {
    router.setMode(InputMode::GameAndUI);
    router.toggleUIMode();
    EXPECT_EQ(router.mode(), InputMode::GameAndUI);
}

// -- SDL key to RmlUI key mapping --

TEST_F(InputRouterTest, KeyMapLetters) {
    EXPECT_EQ(InputRouter::sdlKeyToRmlKey(SDLK_A), Rml::Input::KI_A);
    EXPECT_EQ(InputRouter::sdlKeyToRmlKey(SDLK_Z), Rml::Input::KI_Z);
    EXPECT_EQ(InputRouter::sdlKeyToRmlKey(SDLK_M), Rml::Input::KI_M);
}

TEST_F(InputRouterTest, KeyMapDigits) {
    EXPECT_EQ(InputRouter::sdlKeyToRmlKey(SDLK_0), Rml::Input::KI_0);
    EXPECT_EQ(InputRouter::sdlKeyToRmlKey(SDLK_9), Rml::Input::KI_9);
    EXPECT_EQ(InputRouter::sdlKeyToRmlKey(SDLK_5), Rml::Input::KI_5);
}

TEST_F(InputRouterTest, KeyMapFunctionKeys) {
    EXPECT_EQ(InputRouter::sdlKeyToRmlKey(SDLK_F1), Rml::Input::KI_F1);
    EXPECT_EQ(InputRouter::sdlKeyToRmlKey(SDLK_F12), Rml::Input::KI_F12);
}

TEST_F(InputRouterTest, KeyMapSpecialKeys) {
    EXPECT_EQ(InputRouter::sdlKeyToRmlKey(SDLK_SPACE), Rml::Input::KI_SPACE);
    EXPECT_EQ(InputRouter::sdlKeyToRmlKey(SDLK_RETURN), Rml::Input::KI_RETURN);
    EXPECT_EQ(InputRouter::sdlKeyToRmlKey(SDLK_ESCAPE), Rml::Input::KI_ESCAPE);
    EXPECT_EQ(InputRouter::sdlKeyToRmlKey(SDLK_BACKSPACE), Rml::Input::KI_BACK);
    EXPECT_EQ(InputRouter::sdlKeyToRmlKey(SDLK_TAB), Rml::Input::KI_TAB);
    EXPECT_EQ(InputRouter::sdlKeyToRmlKey(SDLK_DELETE), Rml::Input::KI_DELETE);
}

TEST_F(InputRouterTest, KeyMapArrows) {
    EXPECT_EQ(InputRouter::sdlKeyToRmlKey(SDLK_LEFT), Rml::Input::KI_LEFT);
    EXPECT_EQ(InputRouter::sdlKeyToRmlKey(SDLK_RIGHT), Rml::Input::KI_RIGHT);
    EXPECT_EQ(InputRouter::sdlKeyToRmlKey(SDLK_UP), Rml::Input::KI_UP);
    EXPECT_EQ(InputRouter::sdlKeyToRmlKey(SDLK_DOWN), Rml::Input::KI_DOWN);
}

TEST_F(InputRouterTest, KeyMapNavigation) {
    EXPECT_EQ(InputRouter::sdlKeyToRmlKey(SDLK_HOME), Rml::Input::KI_HOME);
    EXPECT_EQ(InputRouter::sdlKeyToRmlKey(SDLK_END), Rml::Input::KI_END);
    EXPECT_EQ(InputRouter::sdlKeyToRmlKey(SDLK_PAGEUP), Rml::Input::KI_PRIOR);
    EXPECT_EQ(InputRouter::sdlKeyToRmlKey(SDLK_PAGEDOWN), Rml::Input::KI_NEXT);
    EXPECT_EQ(InputRouter::sdlKeyToRmlKey(SDLK_INSERT), Rml::Input::KI_INSERT);
}

TEST_F(InputRouterTest, KeyMapModifierKeys) {
    EXPECT_EQ(InputRouter::sdlKeyToRmlKey(SDLK_LSHIFT), Rml::Input::KI_LSHIFT);
    EXPECT_EQ(InputRouter::sdlKeyToRmlKey(SDLK_RSHIFT), Rml::Input::KI_RSHIFT);
    EXPECT_EQ(InputRouter::sdlKeyToRmlKey(SDLK_LCTRL), Rml::Input::KI_LCONTROL);
    EXPECT_EQ(InputRouter::sdlKeyToRmlKey(SDLK_RCTRL), Rml::Input::KI_RCONTROL);
    EXPECT_EQ(InputRouter::sdlKeyToRmlKey(SDLK_LALT), Rml::Input::KI_LMENU);
    EXPECT_EQ(InputRouter::sdlKeyToRmlKey(SDLK_RALT), Rml::Input::KI_RMENU);
}

TEST_F(InputRouterTest, KeyMapNumpad) {
    EXPECT_EQ(InputRouter::sdlKeyToRmlKey(SDLK_KP_0), Rml::Input::KI_NUMPAD0);
    EXPECT_EQ(InputRouter::sdlKeyToRmlKey(SDLK_KP_9), Rml::Input::KI_NUMPAD9);
    EXPECT_EQ(InputRouter::sdlKeyToRmlKey(SDLK_KP_ENTER), Rml::Input::KI_NUMPADENTER);
    EXPECT_EQ(InputRouter::sdlKeyToRmlKey(SDLK_KP_MULTIPLY), Rml::Input::KI_MULTIPLY);
    EXPECT_EQ(InputRouter::sdlKeyToRmlKey(SDLK_KP_PLUS), Rml::Input::KI_ADD);
    EXPECT_EQ(InputRouter::sdlKeyToRmlKey(SDLK_KP_MINUS), Rml::Input::KI_SUBTRACT);
    EXPECT_EQ(InputRouter::sdlKeyToRmlKey(SDLK_KP_PERIOD), Rml::Input::KI_DECIMAL);
    EXPECT_EQ(InputRouter::sdlKeyToRmlKey(SDLK_KP_DIVIDE), Rml::Input::KI_DIVIDE);
}

TEST_F(InputRouterTest, KeyMapUnknownReturnsUnknown) {
    // An unmapped key should return KI_UNKNOWN
    EXPECT_EQ(InputRouter::sdlKeyToRmlKey(SDLK_UNKNOWN), Rml::Input::KI_UNKNOWN);
}

// -- SDL modifier to RmlUI modifier mapping --

TEST_F(InputRouterTest, ModMapShift) {
    int rmlMod = InputRouter::sdlModToRmlMod(SDL_KMOD_LSHIFT);
    EXPECT_TRUE(rmlMod & Rml::Input::KM_SHIFT);
    EXPECT_FALSE(rmlMod & Rml::Input::KM_CTRL);
}

TEST_F(InputRouterTest, ModMapCtrl) {
    int rmlMod = InputRouter::sdlModToRmlMod(SDL_KMOD_LCTRL);
    EXPECT_TRUE(rmlMod & Rml::Input::KM_CTRL);
}

TEST_F(InputRouterTest, ModMapAlt) {
    int rmlMod = InputRouter::sdlModToRmlMod(SDL_KMOD_LALT);
    EXPECT_TRUE(rmlMod & Rml::Input::KM_ALT);
}

TEST_F(InputRouterTest, ModMapMeta) {
    int rmlMod = InputRouter::sdlModToRmlMod(SDL_KMOD_LGUI);
    EXPECT_TRUE(rmlMod & Rml::Input::KM_META);
}

TEST_F(InputRouterTest, ModMapCombined) {
    auto combined = static_cast<SDL_Keymod>(SDL_KMOD_LCTRL | SDL_KMOD_LSHIFT);
    int rmlMod = InputRouter::sdlModToRmlMod(combined);
    EXPECT_TRUE(rmlMod & Rml::Input::KM_CTRL);
    EXPECT_TRUE(rmlMod & Rml::Input::KM_SHIFT);
    EXPECT_FALSE(rmlMod & Rml::Input::KM_ALT);
}

TEST_F(InputRouterTest, ModMapNone) {
    EXPECT_EQ(InputRouter::sdlModToRmlMod(SDL_KMOD_NONE), 0);
}

TEST_F(InputRouterTest, ModMapCapsLock) {
    int rmlMod = InputRouter::sdlModToRmlMod(SDL_KMOD_CAPS);
    EXPECT_TRUE(rmlMod & Rml::Input::KM_CAPSLOCK);
}

// -- Multiple events in sequence --

TEST_F(InputRouterTest, ModeChangePreservesInputState) {
    inputMgr.bindKey("forward", SDLK_W);

    // Press W in GameOnly
    auto down = makeKeyDown(SDLK_W);
    router.routeEvent(down, nullptr);
    EXPECT_TRUE(inputMgr.isActionActive("forward"));

    // Switch to UIOnly (via Escape)
    auto esc = makeKeyDown(SDLK_ESCAPE);
    router.routeEvent(esc, nullptr);
    EXPECT_EQ(router.mode(), InputMode::UIOnly);

    // W is still "active" in InputManager (key wasn't released)
    EXPECT_TRUE(inputMgr.isActionActive("forward"));
}
