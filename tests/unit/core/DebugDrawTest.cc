#include "fabric/core/DebugDraw.hh"

#include <gtest/gtest.h>

using namespace fabric;

// Flag management tests (no bgfx required)

TEST(DebugDrawTest, DefaultConstructionHasNoFlags) {
    DebugDraw dd;
    EXPECT_EQ(dd.flags(), DebugDrawFlags::None);
    EXPECT_FALSE(dd.isInitialized());
    EXPECT_FALSE(dd.isWireframeEnabled());
}

TEST(DebugDrawTest, SetFlagEnablesFlag) {
    DebugDraw dd;
    dd.setFlag(DebugDrawFlags::Wireframe, true);
    EXPECT_TRUE(dd.hasFlag(DebugDrawFlags::Wireframe));
    EXPECT_TRUE(dd.isWireframeEnabled());
}

TEST(DebugDrawTest, SetFlagDisablesFlag) {
    DebugDraw dd;
    dd.setFlag(DebugDrawFlags::Wireframe, true);
    dd.setFlag(DebugDrawFlags::Wireframe, false);
    EXPECT_FALSE(dd.hasFlag(DebugDrawFlags::Wireframe));
}

TEST(DebugDrawTest, ToggleFlagFlips) {
    DebugDraw dd;
    EXPECT_FALSE(dd.hasFlag(DebugDrawFlags::Wireframe));

    dd.toggleFlag(DebugDrawFlags::Wireframe);
    EXPECT_TRUE(dd.hasFlag(DebugDrawFlags::Wireframe));

    dd.toggleFlag(DebugDrawFlags::Wireframe);
    EXPECT_FALSE(dd.hasFlag(DebugDrawFlags::Wireframe));
}

TEST(DebugDrawTest, ToggleWireframeConvenience) {
    DebugDraw dd;
    dd.toggleWireframe();
    EXPECT_TRUE(dd.isWireframeEnabled());
    dd.toggleWireframe();
    EXPECT_FALSE(dd.isWireframeEnabled());
}

TEST(DebugDrawTest, HasFlagReturnsFalseForUnset) {
    DebugDraw dd;
    EXPECT_FALSE(dd.hasFlag(DebugDrawFlags::Wireframe));
}

TEST(DebugDrawTest, FlagsReturnsCurrentValue) {
    DebugDraw dd;
    dd.setFlag(DebugDrawFlags::Wireframe, true);
    EXPECT_EQ(dd.flags(), DebugDrawFlags::Wireframe);

    dd.setFlag(DebugDrawFlags::Wireframe, false);
    EXPECT_EQ(dd.flags(), DebugDrawFlags::None);
}

// Bitwise operator tests

TEST(DebugDrawFlagsTest, BitwiseOr) {
    auto combined = DebugDrawFlags::None | DebugDrawFlags::Wireframe;
    EXPECT_EQ(combined, DebugDrawFlags::Wireframe);
}

TEST(DebugDrawFlagsTest, BitwiseAnd) {
    auto result = DebugDrawFlags::Wireframe & DebugDrawFlags::Wireframe;
    EXPECT_EQ(result, DebugDrawFlags::Wireframe);

    result = DebugDrawFlags::None & DebugDrawFlags::Wireframe;
    EXPECT_EQ(result, DebugDrawFlags::None);
}

TEST(DebugDrawFlagsTest, BitwiseNot) {
    auto inverted = ~DebugDrawFlags::None;
    EXPECT_NE(inverted, DebugDrawFlags::None);
}

// Lifecycle safety tests (no bgfx; init/shutdown/begin/end are no-ops)

TEST(DebugDrawTest, ShutdownWithoutInitIsSafe) {
    DebugDraw dd;
    dd.shutdown(); // should not crash
    EXPECT_FALSE(dd.isInitialized());
}

TEST(DebugDrawTest, BeginEndWithoutInitAreNoOps) {
    DebugDraw dd;
    dd.begin(0); // should not crash
    dd.end();    // should not crash
}

TEST(DebugDrawTest, ApplyDebugFlagsWithoutInitIsNoOp) {
    DebugDraw dd;
    dd.setFlag(DebugDrawFlags::Wireframe, true);
    dd.applyDebugFlags(); // should not crash without bgfx
}
