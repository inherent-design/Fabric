#include "fabric/core/InputAction.hh"
#include <gtest/gtest.h>

using namespace fabric;

TEST(InputActionTest, BindingCreation) {
    ActionBinding binding;
    binding.name = "jump";
    binding.sources.push_back(KeySource{SDLK_SPACE});

    EXPECT_EQ(binding.name, "jump");
    EXPECT_EQ(binding.sources.size(), 1u);
    EXPECT_FALSE(binding.oneShot);
}

TEST(InputActionTest, MultiSourceBinding) {
    ActionBinding binding;
    binding.name = "jump";
    binding.sources.push_back(KeySource{SDLK_SPACE});
    binding.sources.push_back(GamepadButtonSource{SDL_GAMEPAD_BUTTON_SOUTH});

    EXPECT_EQ(binding.sources.size(), 2u);
    EXPECT_TRUE(std::holds_alternative<KeySource>(binding.sources[0]));
    EXPECT_TRUE(std::holds_alternative<GamepadButtonSource>(binding.sources[1]));
}

TEST(InputActionTest, OneShotFlag) {
    ActionBinding binding;
    binding.name = "screenshot";
    binding.sources.push_back(KeySource{SDLK_F12});
    binding.oneShot = true;

    EXPECT_TRUE(binding.oneShot);
}

TEST(InputActionTest, SnapshotJustPressed) {
    ActionSnapshot snap;
    snap.state = ActionState::JustPressed;

    EXPECT_TRUE(snap.isActive());
    EXPECT_TRUE(snap.justPressed());
    EXPECT_FALSE(snap.justReleased());
}

TEST(InputActionTest, SnapshotHeld) {
    ActionSnapshot snap;
    snap.state = ActionState::Held;

    EXPECT_TRUE(snap.isActive());
    EXPECT_FALSE(snap.justPressed());
    EXPECT_FALSE(snap.justReleased());
}

TEST(InputActionTest, SnapshotJustReleased) {
    ActionSnapshot snap;
    snap.state = ActionState::JustReleased;

    EXPECT_FALSE(snap.isActive());
    EXPECT_FALSE(snap.justPressed());
    EXPECT_TRUE(snap.justReleased());
}

TEST(InputActionTest, SnapshotDefaultReleased) {
    ActionSnapshot snap;

    EXPECT_EQ(snap.state, ActionState::Released);
    EXPECT_FALSE(snap.isActive());
    EXPECT_FALSE(snap.justPressed());
    EXPECT_FALSE(snap.justReleased());
}

// --- Audit: multi-source and edge cases ---

TEST(InputActionTest, ThreeSourceMixedTypes) {
    ActionBinding binding;
    binding.name = "attack";
    binding.sources.push_back(KeySource{SDLK_SPACE});
    binding.sources.push_back(MouseButtonSource{SDL_BUTTON_LEFT});
    binding.sources.push_back(GamepadButtonSource{SDL_GAMEPAD_BUTTON_WEST});

    EXPECT_EQ(binding.sources.size(), 3u);
    EXPECT_TRUE(std::holds_alternative<KeySource>(binding.sources[0]));
    EXPECT_TRUE(std::holds_alternative<MouseButtonSource>(binding.sources[1]));
    EXPECT_TRUE(std::holds_alternative<GamepadButtonSource>(binding.sources[2]));
}

TEST(InputActionTest, EmptySourcesBinding) {
    ActionBinding binding;
    binding.name = "empty_action";
    EXPECT_TRUE(binding.sources.empty());
}

TEST(InputActionTest, BindingNameCanBeEmpty) {
    ActionBinding binding;
    binding.name = "";
    EXPECT_TRUE(binding.name.empty());
}

TEST(InputActionTest, AllActionStatesExhaustive) {
    struct Expect {
        ActionState state;
        bool active;
        bool pressed;
        bool released;
    };
    Expect cases[] = {
        {ActionState::Released, false, false, false},
        {ActionState::JustPressed, true, true, false},
        {ActionState::Held, true, false, false},
        {ActionState::JustReleased, false, false, true},
    };
    for (const auto& c : cases) {
        ActionSnapshot snap;
        snap.state = c.state;
        EXPECT_EQ(snap.isActive(), c.active);
        EXPECT_EQ(snap.justPressed(), c.pressed);
        EXPECT_EQ(snap.justReleased(), c.released);
    }
}
