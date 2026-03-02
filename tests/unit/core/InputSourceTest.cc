#include "fabric/core/InputSource.hh"
#include <gtest/gtest.h>

using namespace fabric;

TEST(InputSourceTest, KeySourceVariant) {
    InputSource src = KeySource{SDLK_W};
    EXPECT_EQ(inputSourceType(src), InputSourceType::Key);
    EXPECT_TRUE(std::holds_alternative<KeySource>(src));
    EXPECT_EQ(std::get<KeySource>(src).key, SDLK_W);
}

TEST(InputSourceTest, MouseButtonSourceVariant) {
    InputSource src = MouseButtonSource{SDL_BUTTON_RIGHT};
    EXPECT_EQ(inputSourceType(src), InputSourceType::MouseButton);
    EXPECT_EQ(std::get<MouseButtonSource>(src).button, SDL_BUTTON_RIGHT);
}

TEST(InputSourceTest, GamepadSourceVariants) {
    InputSource btn = GamepadButtonSource{SDL_GAMEPAD_BUTTON_SOUTH};
    EXPECT_EQ(inputSourceType(btn), InputSourceType::GamepadButton);

    InputSource axis = GamepadAxisSource{SDL_GAMEPAD_AXIS_LEFTX};
    EXPECT_EQ(inputSourceType(axis), InputSourceType::GamepadAxis);
}

TEST(InputSourceTest, MouseAxisAndWheelVariants) {
    InputSource mx = MouseAxisSource{InputAxisComponent::X};
    InputSource my = MouseAxisSource{InputAxisComponent::Y};
    EXPECT_EQ(inputSourceType(mx), InputSourceType::MouseAxis);
    EXPECT_NE(mx, my);

    InputSource wy = MouseWheelSource{InputAxisComponent::Y};
    EXPECT_EQ(inputSourceType(wy), InputSourceType::MouseWheel);
}

TEST(InputSourceTest, VariantEquality) {
    InputSource a = KeySource{SDLK_A};
    InputSource b = KeySource{SDLK_A};
    InputSource c = KeySource{SDLK_B};
    InputSource d = MouseButtonSource{SDL_BUTTON_LEFT};

    EXPECT_EQ(a, b);
    EXPECT_NE(a, c);
    EXPECT_NE(a, d);
}

TEST(InputSourceTest, SourceNameHardcodedTypes) {
    EXPECT_EQ(inputSourceName(MouseButtonSource{SDL_BUTTON_LEFT}), "Mouse Left");
    EXPECT_EQ(inputSourceName(MouseButtonSource{SDL_BUTTON_RIGHT}), "Mouse Right");
    EXPECT_EQ(inputSourceName(MouseAxisSource{InputAxisComponent::X}), "Mouse X");
    EXPECT_EQ(inputSourceName(MouseAxisSource{InputAxisComponent::Y}), "Mouse Y");
    EXPECT_EQ(inputSourceName(MouseWheelSource{InputAxisComponent::X}), "Scroll X");
    EXPECT_EQ(inputSourceName(MouseWheelSource{InputAxisComponent::Y}), "Scroll Y");
}

TEST(InputSourceTest, SourceNameDoesNotCrash) {
    // SDL key/gamepad name functions should return something, even without
    // full SDL initialization. Verify no crash and non-empty result.
    std::string keyName = inputSourceName(KeySource{SDLK_SPACE});
    EXPECT_FALSE(keyName.empty());

    std::string gpBtnName = inputSourceName(GamepadButtonSource{SDL_GAMEPAD_BUTTON_SOUTH});
    EXPECT_FALSE(gpBtnName.empty());

    std::string gpAxisName = inputSourceName(GamepadAxisSource{SDL_GAMEPAD_AXIS_LEFTX});
    EXPECT_FALSE(gpAxisName.empty());
}

// --- Audit: default-constructed source variants ---

TEST(InputSourceTest, DefaultConstructedKeySource) {
    KeySource ks;
    EXPECT_EQ(ks.key, SDLK_UNKNOWN);
}

TEST(InputSourceTest, DefaultConstructedMouseButtonSource) {
    MouseButtonSource mb;
    EXPECT_EQ(mb.button, SDL_BUTTON_LEFT);
}

TEST(InputSourceTest, DefaultConstructedGamepadSources) {
    GamepadButtonSource gb;
    EXPECT_EQ(gb.button, SDL_GAMEPAD_BUTTON_INVALID);

    GamepadAxisSource ga;
    EXPECT_EQ(ga.axis, SDL_GAMEPAD_AXIS_INVALID);
}

// --- Audit: per-type equality ---

TEST(InputSourceTest, MouseButtonSourceEquality) {
    MouseButtonSource a{SDL_BUTTON_LEFT};
    MouseButtonSource b{SDL_BUTTON_LEFT};
    MouseButtonSource c{SDL_BUTTON_RIGHT};
    EXPECT_EQ(a, b);
    EXPECT_NE(a, c);
}

TEST(InputSourceTest, MouseWheelSourceEquality) {
    MouseWheelSource a{InputAxisComponent::X};
    MouseWheelSource b{InputAxisComponent::X};
    MouseWheelSource c{InputAxisComponent::Y};
    EXPECT_EQ(a, b);
    EXPECT_NE(a, c);
}

TEST(InputSourceTest, GamepadButtonSourceEquality) {
    GamepadButtonSource a{SDL_GAMEPAD_BUTTON_SOUTH};
    GamepadButtonSource b{SDL_GAMEPAD_BUTTON_SOUTH};
    GamepadButtonSource c{SDL_GAMEPAD_BUTTON_NORTH};
    EXPECT_EQ(a, b);
    EXPECT_NE(a, c);
}

TEST(InputSourceTest, GamepadAxisSourceEquality) {
    GamepadAxisSource a{SDL_GAMEPAD_AXIS_LEFTX};
    GamepadAxisSource b{SDL_GAMEPAD_AXIS_LEFTX};
    GamepadAxisSource c{SDL_GAMEPAD_AXIS_RIGHTX};
    EXPECT_EQ(a, b);
    EXPECT_NE(a, c);
}

// --- Audit: name edge cases ---

TEST(InputSourceTest, UnknownKeySourceName) {
    std::string name = inputSourceName(KeySource{SDLK_UNKNOWN});
    EXPECT_FALSE(name.empty());
}

TEST(InputSourceTest, InvalidGamepadButtonName) {
    std::string name = inputSourceName(GamepadButtonSource{SDL_GAMEPAD_BUTTON_INVALID});
    EXPECT_FALSE(name.empty());
}

TEST(InputSourceTest, InvalidGamepadAxisName) {
    std::string name = inputSourceName(GamepadAxisSource{SDL_GAMEPAD_AXIS_INVALID});
    EXPECT_FALSE(name.empty());
}

TEST(InputSourceTest, MouseButtonNameMiddle) {
    EXPECT_EQ(inputSourceName(MouseButtonSource{SDL_BUTTON_MIDDLE}), "Mouse Middle");
}

TEST(InputSourceTest, MouseButtonNameX1X2) {
    EXPECT_EQ(inputSourceName(MouseButtonSource{SDL_BUTTON_X1}), "Mouse X1");
    EXPECT_EQ(inputSourceName(MouseButtonSource{SDL_BUTTON_X2}), "Mouse X2");
}

TEST(InputSourceTest, MouseButtonNameOutOfRange) {
    std::string name = inputSourceName(MouseButtonSource{6});
    EXPECT_EQ(name, "Mouse Button 6");
}

// --- Audit: exhaustive type mapping ---

TEST(InputSourceTest, AllSixVariantTypesMap) {
    EXPECT_EQ(inputSourceType(InputSource{KeySource{}}), InputSourceType::Key);
    EXPECT_EQ(inputSourceType(InputSource{MouseButtonSource{}}), InputSourceType::MouseButton);
    EXPECT_EQ(inputSourceType(InputSource{MouseAxisSource{}}), InputSourceType::MouseAxis);
    EXPECT_EQ(inputSourceType(InputSource{MouseWheelSource{}}), InputSourceType::MouseWheel);
    EXPECT_EQ(inputSourceType(InputSource{GamepadButtonSource{}}), InputSourceType::GamepadButton);
    EXPECT_EQ(inputSourceType(InputSource{GamepadAxisSource{}}), InputSourceType::GamepadAxis);
}

TEST(InputSourceTest, CrossTypeVariantInequality) {
    InputSource key = KeySource{};
    InputSource mouseBtn = MouseButtonSource{};
    InputSource mouseAxis = MouseAxisSource{};
    InputSource mouseWheel = MouseWheelSource{};
    InputSource gpBtn = GamepadButtonSource{};
    InputSource gpAxis = GamepadAxisSource{};

    EXPECT_NE(key, mouseBtn);
    EXPECT_NE(key, mouseAxis);
    EXPECT_NE(key, mouseWheel);
    EXPECT_NE(key, gpBtn);
    EXPECT_NE(key, gpAxis);
    EXPECT_NE(mouseBtn, mouseAxis);
    EXPECT_NE(mouseBtn, mouseWheel);
    EXPECT_NE(gpBtn, gpAxis);
}
