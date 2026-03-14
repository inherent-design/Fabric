#include "fabric/utils/ErrorHandling.hh"
#include "gtest/gtest.h"
#include <string>

class ErrorHandlingTest : public ::testing::Test {};

TEST_F(ErrorHandlingTest, TestFabricExceptionConstruction) {
    fabric::FabricException exception("Test error message");
    ASSERT_STREQ("Test error message", exception.what());
}

TEST_F(ErrorHandlingTest, TestThrowError) {
    try {
        fabric::throwError("Test error message");
        FAIL() << "Expected FabricException";
    } catch (const fabric::FabricException& e) {
        ASSERT_STREQ("Test error message", e.what());
    } catch (...) {
        FAIL() << "Expected FabricException, got a different exception";
    }
}
