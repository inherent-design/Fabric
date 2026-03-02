#pragma once

#include <gtest/gtest.h>
#include <SDL3/SDL.h>

namespace fabric::test {

// Test fixture that initializes SDL video subsystem.
// Skips the test when no display server is available (headless CI).
class SDLFixture : public ::testing::Test {
  protected:
    void SetUp() override {
        if (!SDL_Init(SDL_INIT_VIDEO)) {
            GTEST_SKIP() << "SDL_Init(SDL_INIT_VIDEO) failed: no display server available.";
        }
    }

    void TearDown() override { SDL_Quit(); }
};

} // namespace fabric::test
