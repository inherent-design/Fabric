#include "fabric/platform/CursorManager.hh"

#include <gtest/gtest.h>

using namespace fabric;

// CursorManager is constructed with nullptr for unit tests.
// SDL calls will fail silently but mode tracking still works.

TEST(CursorManagerTest, InitialModeIsNormal) {
    CursorManager mgr(nullptr);
    EXPECT_EQ(mgr.currentMode(), CursorMode::Normal);
}

TEST(CursorManagerTest, SetModeCaptured) {
    CursorManager mgr(nullptr);
    mgr.setMode(CursorMode::Captured);
    EXPECT_EQ(mgr.currentMode(), CursorMode::Captured);
}

TEST(CursorManagerTest, SetModeConfined) {
    CursorManager mgr(nullptr);
    mgr.setMode(CursorMode::Confined);
    EXPECT_EQ(mgr.currentMode(), CursorMode::Confined);
}

TEST(CursorManagerTest, SetModeSameIsNoOp) {
    CursorManager mgr(nullptr);
    mgr.setMode(CursorMode::Normal);
    EXPECT_EQ(mgr.currentMode(), CursorMode::Normal);
}

TEST(CursorManagerTest, ApplyCaptureFlagTrue) {
    CursorManager mgr(nullptr);
    mgr.applyCaptureFlag(true);
    EXPECT_EQ(mgr.currentMode(), CursorMode::Captured);
}

TEST(CursorManagerTest, ApplyCaptureFlagFalse) {
    CursorManager mgr(nullptr);
    mgr.setMode(CursorMode::Captured);
    mgr.applyCaptureFlag(false);
    EXPECT_EQ(mgr.currentMode(), CursorMode::Normal);
}
