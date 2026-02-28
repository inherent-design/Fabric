#include "fabric/core/Log.hh"
#include <gtest/gtest.h>

int main(int argc, char** argv) {
    fabric::log::init();
    ::testing::InitGoogleTest(&argc, argv);
    int result = RUN_ALL_TESTS();
    fabric::log::shutdown();
    return result;
}
