#include "fabric/core/InputContext.hh"
#include <gtest/gtest.h>

using namespace fabric;

TEST(InputContextTest, ConstructionDefaults) {
    InputContext ctx("gameplay", 100);
    EXPECT_EQ(ctx.name(), "gameplay");
    EXPECT_EQ(ctx.priority(), 100);
    EXPECT_TRUE(ctx.consumeInput());
    EXPECT_FALSE(ctx.routeToUI());
    EXPECT_TRUE(ctx.enabled());
    EXPECT_TRUE(ctx.actions().empty());
    EXPECT_TRUE(ctx.axes().empty());
}

TEST(InputContextTest, AddAndFindAction) {
    InputContext ctx("test");

    ActionBinding jump;
    jump.name = "jump";
    jump.sources.push_back(KeySource{SDLK_SPACE});
    ctx.addAction(jump);

    EXPECT_EQ(ctx.actions().size(), 1u);

    const auto* found = ctx.findAction("jump");
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->name, "jump");
    EXPECT_EQ(found->sources.size(), 1u);

    EXPECT_EQ(ctx.findAction("nonexistent"), nullptr);
}

TEST(InputContextTest, AddAndFindAxis) {
    InputContext ctx("test");

    AxisBinding moveX;
    moveX.name = "move_x";
    moveX.deadZone = 0.15f;
    ctx.addAxis(moveX);

    EXPECT_EQ(ctx.axes().size(), 1u);

    const auto* found = ctx.findAxis("move_x");
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->name, "move_x");
    EXPECT_FLOAT_EQ(found->deadZone, 0.15f);

    EXPECT_EQ(ctx.findAxis("nonexistent"), nullptr);
}

TEST(InputContextTest, RemoveAction) {
    InputContext ctx("test");

    ActionBinding a;
    a.name = "a";
    ctx.addAction(a);

    ActionBinding b;
    b.name = "b";
    ctx.addAction(b);

    EXPECT_EQ(ctx.actions().size(), 2u);

    ctx.removeAction("a");
    EXPECT_EQ(ctx.actions().size(), 1u);
    EXPECT_EQ(ctx.findAction("a"), nullptr);
    EXPECT_NE(ctx.findAction("b"), nullptr);

    // Remove nonexistent is safe
    ctx.removeAction("nonexistent");
    EXPECT_EQ(ctx.actions().size(), 1u);
}

TEST(InputContextTest, RemoveAxis) {
    InputContext ctx("test");

    AxisBinding x;
    x.name = "x";
    ctx.addAxis(x);

    AxisBinding y;
    y.name = "y";
    ctx.addAxis(y);

    ctx.removeAxis("x");
    EXPECT_EQ(ctx.axes().size(), 1u);
    EXPECT_EQ(ctx.findAxis("x"), nullptr);
    EXPECT_NE(ctx.findAxis("y"), nullptr);
}

TEST(InputContextTest, ClearRemovesAll) {
    InputContext ctx("test");

    ActionBinding action;
    action.name = "jump";
    ctx.addAction(action);

    AxisBinding axis;
    axis.name = "move_x";
    ctx.addAxis(axis);

    ctx.clear();
    EXPECT_TRUE(ctx.actions().empty());
    EXPECT_TRUE(ctx.axes().empty());
    EXPECT_EQ(ctx.findAction("jump"), nullptr);
    EXPECT_EQ(ctx.findAxis("move_x"), nullptr);
}

TEST(InputContextTest, RebindAction) {
    InputContext ctx("test");

    ActionBinding jump;
    jump.name = "jump";
    jump.sources.push_back(KeySource{SDLK_SPACE});
    ctx.addAction(jump);

    std::vector<InputSource> newSources = {KeySource{SDLK_UP}, GamepadButtonSource{SDL_GAMEPAD_BUTTON_SOUTH}};
    EXPECT_TRUE(ctx.rebindAction("jump", newSources));
    EXPECT_EQ(ctx.findAction("jump")->sources.size(), 2u);

    EXPECT_FALSE(ctx.rebindAction("nonexistent", newSources));
}

TEST(InputContextTest, RebindAxis) {
    InputContext ctx("test");

    AxisBinding moveX;
    moveX.name = "move_x";
    AxisSource src;
    src.useKeyPair = true;
    src.keyPair = {SDLK_A, SDLK_D};
    moveX.sources.push_back(src);
    ctx.addAxis(moveX);

    AxisSource newSrc;
    newSrc.source = GamepadAxisSource{SDL_GAMEPAD_AXIS_LEFTX};
    std::vector<AxisSource> newSources = {newSrc};
    EXPECT_TRUE(ctx.rebindAxis("move_x", newSources));
    EXPECT_EQ(ctx.findAxis("move_x")->sources.size(), 1u);

    EXPECT_FALSE(ctx.rebindAxis("nonexistent", newSources));
}

TEST(InputContextTest, PriorityOrdering) {
    InputContext global("global", 0);
    InputContext gameplay("gameplay", 100);
    InputContext menu("menu", 200);

    EXPECT_LT(global.priority(), gameplay.priority());
    EXPECT_LT(gameplay.priority(), menu.priority());
}

TEST(InputContextTest, EnableDisable) {
    InputContext ctx("test");
    EXPECT_TRUE(ctx.enabled());

    ctx.setEnabled(false);
    EXPECT_FALSE(ctx.enabled());

    ctx.setEnabled(true);
    EXPECT_TRUE(ctx.enabled());
}

TEST(InputContextTest, ConsumeAndRouteFlags) {
    InputContext ctx("menu", 200);
    ctx.setConsumeInput(true);
    ctx.setRouteToUI(true);

    EXPECT_TRUE(ctx.consumeInput());
    EXPECT_TRUE(ctx.routeToUI());

    ctx.setConsumeInput(false);
    ctx.setRouteToUI(false);
    EXPECT_FALSE(ctx.consumeInput());
    EXPECT_FALSE(ctx.routeToUI());
}

// --- Audit: edge cases ---

TEST(InputContextTest, DefaultPriorityIsZero) {
    InputContext ctx("default_test");
    EXPECT_EQ(ctx.priority(), 0);
}

TEST(InputContextTest, AddActionDuplicateNameOverwritesInPlace) {
    InputContext ctx("test");

    ActionBinding first;
    first.name = "Jump";
    first.sources.push_back(KeySource{SDLK_SPACE});
    ctx.addAction(first);

    ActionBinding second;
    second.name = "Jump";
    second.sources.push_back(KeySource{SDLK_UP});
    ctx.addAction(second);

    // Duplicate name overwrites in place: only one entry
    EXPECT_EQ(ctx.actions().size(), 1u);
    // findAction returns the updated binding with source B
    const auto* found = ctx.findAction("Jump");
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->sources.size(), 1u);
    EXPECT_EQ(std::get<KeySource>(found->sources[0]).key, SDLK_UP);

    // Clean removal: no orphans
    ctx.removeAction("Jump");
    EXPECT_EQ(ctx.actions().size(), 0u);
    EXPECT_EQ(ctx.findAction("Jump"), nullptr);
}

TEST(InputContextTest, RemoveMiddleActionRebuildsIndex) {
    InputContext ctx("test");

    ActionBinding a;
    a.name = "a";
    ctx.addAction(a);
    ActionBinding b;
    b.name = "b";
    ctx.addAction(b);
    ActionBinding c;
    c.name = "c";
    ctx.addAction(c);

    ctx.removeAction("b");
    EXPECT_EQ(ctx.actions().size(), 2u);
    EXPECT_NE(ctx.findAction("a"), nullptr);
    EXPECT_EQ(ctx.findAction("b"), nullptr);
    EXPECT_NE(ctx.findAction("c"), nullptr);
}

TEST(InputContextTest, RemoveMiddleAxisRebuildsIndex) {
    InputContext ctx("test");

    AxisBinding x;
    x.name = "x";
    ctx.addAxis(x);
    AxisBinding y;
    y.name = "y";
    ctx.addAxis(y);
    AxisBinding z;
    z.name = "z";
    ctx.addAxis(z);

    ctx.removeAxis("y");
    EXPECT_EQ(ctx.axes().size(), 2u);
    EXPECT_NE(ctx.findAxis("x"), nullptr);
    EXPECT_EQ(ctx.findAxis("y"), nullptr);
    EXPECT_NE(ctx.findAxis("z"), nullptr);
}

TEST(InputContextTest, RebindActionToEmptySources) {
    InputContext ctx("test");

    ActionBinding jump;
    jump.name = "jump";
    jump.sources.push_back(KeySource{SDLK_SPACE});
    ctx.addAction(jump);

    std::vector<InputSource> empty;
    EXPECT_TRUE(ctx.rebindAction("jump", empty));
    EXPECT_TRUE(ctx.findAction("jump")->sources.empty());
}

TEST(InputContextTest, RebindAxisToEmptySources) {
    InputContext ctx("test");

    AxisBinding moveX;
    moveX.name = "move_x";
    AxisSource src;
    src.useKeyPair = true;
    src.keyPair = {SDLK_A, SDLK_D};
    moveX.sources.push_back(src);
    ctx.addAxis(moveX);

    std::vector<AxisSource> empty;
    EXPECT_TRUE(ctx.rebindAxis("move_x", empty));
    EXPECT_TRUE(ctx.findAxis("move_x")->sources.empty());
}

TEST(InputContextTest, AddAfterClear) {
    InputContext ctx("test");

    ActionBinding a;
    a.name = "a";
    ctx.addAction(a);
    ctx.clear();

    ActionBinding b;
    b.name = "b";
    ctx.addAction(b);

    EXPECT_EQ(ctx.actions().size(), 1u);
    EXPECT_NE(ctx.findAction("b"), nullptr);
    EXPECT_EQ(ctx.findAction("a"), nullptr);
}

TEST(InputContextTest, NegativePriority) {
    InputContext ctx("background", -10);
    EXPECT_EQ(ctx.priority(), -10);
}
