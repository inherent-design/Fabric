#include "fabric/core/Log.hh"
#include <gtest/gtest.h>

namespace fabric {
namespace Tests {

class LoggingTest : public ::testing::Test {};

TEST_F(LoggingTest, LogInfoDoesNotCrash) {
    FABRIC_LOG_INFO("Info message");
    EXPECT_TRUE(true);
}

TEST_F(LoggingTest, LogWarningDoesNotCrash) {
    FABRIC_LOG_WARN("Warning message");
    EXPECT_TRUE(true);
}

TEST_F(LoggingTest, LogErrorDoesNotCrash) {
    FABRIC_LOG_ERROR("Error message");
    EXPECT_TRUE(true);
}

TEST_F(LoggingTest, LogDebugDoesNotCrash) {
    FABRIC_LOG_DEBUG("Debug message");
    EXPECT_TRUE(true);
}

TEST_F(LoggingTest, LogWithFormatArgs) {
    FABRIC_LOG_INFO("Value: {}, Name: {}", 42, "test");
    EXPECT_TRUE(true);
}

TEST_F(LoggingTest, SetLogLevel) {
    fabric::log::setLevel(quill::LogLevel::Warning);
    FABRIC_LOG_WARN("This should appear");
    // Reset to default
    fabric::log::setLevel(quill::LogLevel::Debug);
    EXPECT_TRUE(true);
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

    EXPECT_TRUE(true);
}

TEST_F(LoggingTest, LogMultipleFormatTypes) {
    FABRIC_LOG_INFO("int={} float={:.2f} string={} bool={}", 42, 3.14, "hello", true);
    EXPECT_TRUE(true);
}

} // namespace Tests
} // namespace fabric
