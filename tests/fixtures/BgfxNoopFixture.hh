#pragma once

#include <bgfx/bgfx.h>
#include <bgfx/platform.h>
#include <gtest/gtest.h>

namespace fabric::test {

// GoogleTest environment that initializes bgfx with the Noop renderer exactly
// once per process. Register via AddGlobalTestEnvironment in TestMain.cc.
class BgfxNoopEnvironment : public ::testing::Environment {
  public:
    void SetUp() override {
        if (initialized_)
            return;

        bgfx::renderFrame();

        bgfx::Init init;
        init.type = bgfx::RendererType::Noop;
        init.resolution.width = 320;
        init.resolution.height = 240;

        if (bgfx::init(init)) {
            initialized_ = true;
        }
    }

    void TearDown() override {
        if (initialized_) {
            bgfx::shutdown();
            initialized_ = false;
        }
    }

    static bool initialized() { return initialized_; }

  private:
    static inline bool initialized_ = false;
};

// Test fixture for tests that need the bgfx Noop renderer. Assumes
// BgfxNoopEnvironment is registered in TestMain.cc. Skips the test
// if bgfx init failed.
class BgfxNoopFixture : public ::testing::Test {
  protected:
    void SetUp() override {
        if (!BgfxNoopEnvironment::initialized()) {
            GTEST_SKIP() << "bgfx::init(Noop) failed; skipping bgfx-dependent test.";
        }
    }
};

} // namespace fabric::test
