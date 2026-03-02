#pragma once

#include <bgfx/bgfx.h>
#include <bgfx/platform.h>
#include <gtest/gtest.h>

namespace fabric::test {

// Test fixture that initializes bgfx with the Noop renderer once per suite.
// No GPU or display server required. The noop backend creates valid handles
// for uniforms and buffers but does not execute real rendering.
//
// bgfx only supports one init/shutdown cycle per process without thread-ID
// confusion, so we use SetUpTestSuite/TearDownTestSuite (static, once per
// linked suite) rather than per-test SetUp/TearDown.
class BgfxNoopFixture : public ::testing::Test {
  protected:
    static void SetUpTestSuite() {
        if (initialized_) {
            return;
        }

        bgfx::renderFrame();

        bgfx::Init init;
        init.type = bgfx::RendererType::Noop;
        init.resolution.width = 320;
        init.resolution.height = 240;

        if (bgfx::init(init)) {
            initialized_ = true;
        }
    }

    static void TearDownTestSuite() {
        if (initialized_) {
            bgfx::shutdown();
            initialized_ = false;
        }
    }

    void SetUp() override {
        if (!initialized_) {
            GTEST_SKIP() << "bgfx::init(Noop) failed; skipping bgfx-dependent test.";
        }
    }

  private:
    static inline bool initialized_ = false;
};

} // namespace fabric::test
