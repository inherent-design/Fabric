#include "fabric/log/Log.hh"
#include "fixtures/BgfxNoopFixture.hh"
#include <gtest/gtest.h>

int main(int argc, char** argv) {
    fabric::log::init();
    ::testing::InitGoogleTest(&argc, argv);
    ::testing::AddGlobalTestEnvironment(new fabric::test::BgfxNoopEnvironment());
    int result = RUN_ALL_TESTS();
    fabric::log::shutdown();
    return result;
}
