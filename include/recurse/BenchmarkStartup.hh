#pragma once

#include <string>
#include <vector>

namespace recurse {

struct PreparedStartupArguments {
    std::vector<std::string> storage;
    std::vector<char*> argv;

    [[nodiscard]] int argc() const { return static_cast<int>(argv.size()); }
    [[nodiscard]] char** data() { return argv.data(); }
};

PreparedStartupArguments prepareStartupArguments(int argc, char* argv[]);

} // namespace recurse
