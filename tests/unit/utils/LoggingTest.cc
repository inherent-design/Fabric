#include "fabric/log/Log.hh"
#include <gtest/gtest.h>

namespace fabric {
namespace Tests {

class LoggingTest : public ::testing::Test {};

TEST_F(LoggingTest, LogInfoDoesNotCrash) {
    // Verify logger is accessible before logging
    EXPECT_NE(fabric::log::logger(), nullptr);
    // Log macro executes without throwing (logging is designed to be noexcept)
    FABRIC_LOG_INFO("Info message");
}

TEST_F(LoggingTest, LogWarningDoesNotCrash) {
    EXPECT_NE(fabric::log::logger(), nullptr);
    FABRIC_LOG_WARN("Warning message");
}

TEST_F(LoggingTest, LogErrorDoesNotCrash) {
    EXPECT_NE(fabric::log::logger(), nullptr);
    FABRIC_LOG_ERROR("Error message");
}

TEST_F(LoggingTest, LogDebugDoesNotCrash) {
    EXPECT_NE(fabric::log::logger(), nullptr);
    FABRIC_LOG_DEBUG("Debug message");
}

TEST_F(LoggingTest, LogWithFormatArgs) {
    EXPECT_NE(fabric::log::logger(), nullptr);
    // Verify formatted logging with various arg types
    FABRIC_LOG_INFO("Value: {}, Name: {}", 42, "test");
}

TEST_F(LoggingTest, SetLogLevel) {
    // Verify level changes are accepted without throwing
    fabric::log::setLevel(quill::LogLevel::Warning);
    fabric::log::setLevel(quill::LogLevel::Debug);

    // Verify subsystem loggers are accessible
    EXPECT_NE(fabric::log::renderLogger(), nullptr);
    EXPECT_NE(fabric::log::bgfxLogger(), nullptr);
}

TEST_F(LoggingTest, LoggerReturnsNonNull) {
    quill::Logger* root = fabric::log::logger();
    EXPECT_NE(root, nullptr);
}

TEST_F(LoggingTest, SubsystemLoggersReturnNonNull) {
    EXPECT_NE(fabric::log::renderLogger(), nullptr);
    EXPECT_NE(fabric::log::physicsLogger(), nullptr);
    EXPECT_NE(fabric::log::audioLogger(), nullptr);
    EXPECT_NE(fabric::log::bgfxLogger(), nullptr);
}

TEST_F(LoggingTest, SetSubsystemLevelsDoNotCrash) {
    // Verify all subsystem level setters work
    fabric::log::setRenderLevel(quill::LogLevel::Error);
    fabric::log::setPhysicsLevel(quill::LogLevel::Warning);
    fabric::log::setAudioLevel(quill::LogLevel::Info);
    fabric::log::setBgfxLevel(quill::LogLevel::Debug);

    // Subsystem loggers should accept messages at their level
    FABRIC_LOG_RENDER_ERROR("Render error test");
    FABRIC_LOG_PHYSICS_WARN("Physics warn test");
    FABRIC_LOG_AUDIO_INFO("Audio info test");
    FABRIC_LOG_BGFX_DEBUG("Bgfx debug test");

    // Reset levels
    fabric::log::setRenderLevel(quill::LogLevel::Debug);
    fabric::log::setPhysicsLevel(quill::LogLevel::Debug);
    fabric::log::setAudioLevel(quill::LogLevel::Debug);
    fabric::log::setBgfxLevel(quill::LogLevel::Debug);
}

TEST_F(LoggingTest, LogMultipleFormatTypes) {
    EXPECT_NE(fabric::log::logger(), nullptr);
    // Verify logging with multiple format types
    FABRIC_LOG_INFO("int={} float={:.2f} string={} bool={}", 42, 3.14, "hello", true);
}

} // namespace Tests
} // namespace fabric
