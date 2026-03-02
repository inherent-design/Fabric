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
