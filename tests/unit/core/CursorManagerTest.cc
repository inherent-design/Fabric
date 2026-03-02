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

// -- Rapid mode transitions --

TEST(CursorManagerTest, RapidModeTransitions) {
    CursorManager mgr(nullptr);

    mgr.setMode(CursorMode::Normal);
    mgr.setMode(CursorMode::Captured);
    mgr.setMode(CursorMode::Confined);
    mgr.setMode(CursorMode::Normal);
    mgr.setMode(CursorMode::Captured);

    EXPECT_EQ(mgr.currentMode(), CursorMode::Captured);
}

TEST(CursorManagerTest, FullModeCycle) {
    CursorManager mgr(nullptr);

    // Normal -> Captured -> Confined -> Normal
    EXPECT_EQ(mgr.currentMode(), CursorMode::Normal);

    mgr.setMode(CursorMode::Captured);
    EXPECT_EQ(mgr.currentMode(), CursorMode::Captured);

    mgr.setMode(CursorMode::Confined);
    EXPECT_EQ(mgr.currentMode(), CursorMode::Confined);

    mgr.setMode(CursorMode::Normal);
    EXPECT_EQ(mgr.currentMode(), CursorMode::Normal);
}

// -- applyCaptureFlag from Confined state --

TEST(CursorManagerTest, ApplyCaptureFlagFromConfined) {
    CursorManager mgr(nullptr);
    mgr.setMode(CursorMode::Confined);

    // applyCaptureFlag(true) should transition to Captured
    mgr.applyCaptureFlag(true);
    EXPECT_EQ(mgr.currentMode(), CursorMode::Captured);

    // Reset to Confined
    mgr.setMode(CursorMode::Confined);

    // applyCaptureFlag(false) should transition to Normal (not stay Confined)
    mgr.applyCaptureFlag(false);
    EXPECT_EQ(mgr.currentMode(), CursorMode::Normal);
}

// -- Idempotent mode transitions --

TEST(CursorManagerTest, ApplyCaptureFlagTrueWhenAlreadyCaptured) {
    CursorManager mgr(nullptr);
    mgr.setMode(CursorMode::Captured);

    // Should be a no-op
    mgr.applyCaptureFlag(true);
    EXPECT_EQ(mgr.currentMode(), CursorMode::Captured);
}

TEST(CursorManagerTest, ApplyCaptureFlagFalseWhenAlreadyNormal) {
    CursorManager mgr(nullptr);
    EXPECT_EQ(mgr.currentMode(), CursorMode::Normal);

    // Should be a no-op
    mgr.applyCaptureFlag(false);
    EXPECT_EQ(mgr.currentMode(), CursorMode::Normal);
}

// -- Repeated same-mode transitions --

TEST(CursorManagerTest, RepeatedSameModeIsNoOp) {
    CursorManager mgr(nullptr);

    mgr.setMode(CursorMode::Captured);
    mgr.setMode(CursorMode::Captured);
    mgr.setMode(CursorMode::Captured);
    EXPECT_EQ(mgr.currentMode(), CursorMode::Captured);

    mgr.setMode(CursorMode::Confined);
    mgr.setMode(CursorMode::Confined);
    EXPECT_EQ(mgr.currentMode(), CursorMode::Confined);
}

// -- Alternating capture flag --

TEST(CursorManagerTest, AlternatingCaptureFlag) {
    CursorManager mgr(nullptr);

    for (int i = 0; i < 10; ++i) {
        mgr.applyCaptureFlag(true);
        EXPECT_EQ(mgr.currentMode(), CursorMode::Captured);
        mgr.applyCaptureFlag(false);
        EXPECT_EQ(mgr.currentMode(), CursorMode::Normal);
    }
}

// -- CursorMode enum values --

TEST(CursorManagerTest, CursorModeEnumValues) {
    EXPECT_EQ(static_cast<uint8_t>(CursorMode::Normal), 0);
    EXPECT_EQ(static_cast<uint8_t>(CursorMode::Captured), 1);
    EXPECT_EQ(static_cast<uint8_t>(CursorMode::Confined), 2);
}
