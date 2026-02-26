#include "fabric/core/MovementFSM.hh"

#include <gtest/gtest.h>

using namespace fabric;

namespace {

constexpr const char* kSprint7StubMessage =
    "Sprint 7 stub: implement concrete runtime integration assertions for this seam.";

} // namespace

TEST(Sprint7StubTest, DISABLED_JoltCharacterBridgeHook) {
    GTEST_SKIP() << kSprint7StubMessage;
}

TEST(Sprint7StubTest, DISABLED_BehaviorTreeRuntimeHook) {
    GTEST_SKIP() << kSprint7StubMessage;
}

TEST(Sprint7StubTest, DISABLED_MiniaudioPlaybackLifecycleHook) {
    GTEST_SKIP() << kSprint7StubMessage;
}

TEST(Sprint7StubTest, Sprint7ScaffoldCompilesAgainstCoreRuntimeTypes) {
    MovementFSM fsm;
    EXPECT_TRUE(fsm.isGrounded());
}
