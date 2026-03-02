#include "fabric/core/Constants.g.hh"
#include <array>
#include <cstdlib>
#include <fstream>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <iostream>
#include <string>

namespace fabric {
namespace Tests {

// Helper function to execute a command and get output
std::string executeCommand(const std::string& command) {
    std::array<char, 128> buffer;
    std::string result;

#ifdef _WIN32
    FILE* pipe = _popen(command.c_str(), "r");
#else
    FILE* pipe = popen(command.c_str(), "r");
#endif

    if (!pipe) {
        return "ERROR: Command execution failed";
    }

    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        result += buffer.data();
    }

#ifdef _WIN32
    _pclose(pipe);
#else
    pclose(pipe);
#endif

    return result;
}

class FabricE2ETest : public ::testing::Test {
  protected:
    // Path to the Recurse executable (will be built before tests run)
    std::string executablePath;

    void SetUp() override {
        // Determine the path to the Recurse executable
        // This assumes tests run from the build directory
#ifdef _WIN32
        executablePath = "bin\\Recurse.exe";
#else
        executablePath = "bin/Recurse";
#endif

        // Verify that the executable exists
        std::ifstream execFile(executablePath);
        if (!execFile.good()) {
            FAIL() << "Recurse executable not found at: " << executablePath;
        }
    }
};

// Test help flag
TEST_F(FabricE2ETest, HelpFlag) {
    std::string output = executeCommand(executablePath + " --help");

    ASSERT_THAT(output, ::testing::HasSubstr("Usage: " + std::string(fabric::APP_NAME)));
}

// Test version flag
TEST_F(FabricE2ETest, VersionFlag) {
    std::string output = executeCommand(executablePath + " --version");

    ASSERT_THAT(output, ::testing::HasSubstr(fabric::APP_NAME));
    ASSERT_THAT(output, ::testing::HasSubstr(fabric::APP_VERSION));
}

// Note: These tests require the Recurse executable to be built first.
// With log::shutdown() on early exit paths, they no longer SIGTRAP.

} // namespace Tests
} // namespace fabric
