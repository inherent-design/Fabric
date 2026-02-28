#include "fabric/core/Log.hh"
#include <gtest/gtest.h>

namespace fabric {
namespace Tests {

class LoggingTest : public ::testing::Test {};

TEST_F(LoggingTest, LogInfoDoesNotCrash) {
    FABRIC_LOG_INFO("Info message");
    ASSERT_TRUE(true);
}

TEST_F(LoggingTest, LogWarningDoesNotCrash) {
    FABRIC_LOG_WARN("Warning message");
    ASSERT_TRUE(true);
}

TEST_F(LoggingTest, LogErrorDoesNotCrash) {
    FABRIC_LOG_ERROR("Error message");
    ASSERT_TRUE(true);
}

TEST_F(LoggingTest, LogDebugDoesNotCrash) {
    FABRIC_LOG_DEBUG("Debug message");
    ASSERT_TRUE(true);
}

TEST_F(LoggingTest, LogWithFormatArgs) {
    FABRIC_LOG_INFO("Value: {}, Name: {}", 42, "test");
    ASSERT_TRUE(true);
}

TEST_F(LoggingTest, SetLogLevel) {
    fabric::log::setLevel(quill::LogLevel::Warning);
    FABRIC_LOG_WARN("This should appear");
    // Reset to default
    fabric::log::setLevel(quill::LogLevel::Debug);
    ASSERT_TRUE(true);
}

} // namespace Tests
} // namespace fabric
