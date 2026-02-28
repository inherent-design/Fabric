#include "fabric/parser/ArgumentParser.hh"
#include "fabric/core/Log.hh"
#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace fabric {
namespace Tests {

class ArgumentParserTest : public ::testing::Test {
  protected:
    ArgumentParser parser;
};

TEST_F(ArgumentParserTest, ParseSimpleFlag) {
    // Setup
    parser.addArgument("--test", "Test flag");

    // Prepare args
    const char* args[] = {"program", "--test"};
    int argc = sizeof(args) / sizeof(args[0]);

    // Execute
    parser.parse(argc, const_cast<char**>(args));

    // Verify
    ASSERT_TRUE(parser.hasArgument("--test"));
    auto token = parser.getArgument("--test");
    ASSERT_TRUE(token.has_value());
    ASSERT_TRUE(std::get<bool>(token.value().value));
}

TEST_F(ArgumentParserTest, ParseFlagWithValue) {
    // Setup
    parser.addArgument("--name", "Name parameter");

    // Prepare args
    const char* args[] = {"program", "--name", "TestValue"};
    int argc = sizeof(args) / sizeof(args[0]);

    // Execute
    parser.parse(argc, const_cast<char**>(args));

    // Verify
    ASSERT_TRUE(parser.hasArgument("--name"));
    auto token = parser.getArgument("--name");
    ASSERT_TRUE(token.has_value());
    ASSERT_EQ(std::get<std::string>(token.value().value), "TestValue");
}

TEST_F(ArgumentParserTest, ParseMultipleArguments) {
    // Setup
    parser.addArgument("--flag", "Boolean flag");
    parser.addArgument("--param", "String parameter");

    // Prepare args
    const char* args[] = {"program", "--flag", "--param", "Value"};
    int argc = sizeof(args) / sizeof(args[0]);

    // Execute
    parser.parse(argc, const_cast<char**>(args));

    // Verify
    ASSERT_TRUE(parser.hasArgument("--flag"));
    ASSERT_TRUE(parser.hasArgument("--param"));

    auto flagToken = parser.getArgument("--flag");
    ASSERT_TRUE(flagToken.has_value());
    ASSERT_TRUE(std::get<bool>(flagToken.value().value));

    auto paramToken = parser.getArgument("--param");
    ASSERT_TRUE(paramToken.has_value());
    ASSERT_EQ(std::get<std::string>(paramToken.value().value), "Value");
}

TEST_F(ArgumentParserTest, MissingArgument) {
    // Setup
    parser.addArgument("--required", "Required parameter", true);

    // Prepare args
    const char* args[] = {"program"};
    int argc = sizeof(args) / sizeof(args[0]);

    // Execute
    parser.parse(argc, const_cast<char**>(args));

    // Verify
    ASSERT_FALSE(parser.isValid());
}

} // namespace Tests
} // namespace fabric
