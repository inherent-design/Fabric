#include "recurse/BenchmarkStartup.hh"

#include <gtest/gtest.h>

#include <cstdlib>
#include <optional>
#include <string>
#include <vector>

namespace {

struct ScopedEnvVar {
    explicit ScopedEnvVar(const char* name) : name(name) {
        const char* current = std::getenv(name);
        if (current)
            original = current;
    }

    ~ScopedEnvVar() {
#if defined(_WIN32)
        _putenv_s(name, original ? original->c_str() : "");
#else
        if (original) {
            setenv(name, original->c_str(), 1);
        } else {
            unsetenv(name);
        }
#endif
    }

    void set(const char* value) {
#if defined(_WIN32)
        _putenv_s(name, value);
#else
        setenv(name, value, 1);
#endif
    }

    const char* name;
    std::optional<std::string> original;
};

std::vector<char*> toArgv(std::vector<std::string>& args) {
    std::vector<char*> argv;
    argv.reserve(args.size());
    for (auto& arg : args)
        argv.push_back(arg.data());
    return argv;
}

} // namespace

TEST(RecurseBenchmarkStartupTest, BareBenchmarkFlagEnablesAutostartAndSkipSplash) {
    ScopedEnvVar benchmarkEnv("RECURSE_BENCHMARK");
#if defined(_WIN32)
    benchmarkEnv.set("");
#else
    unsetenv("RECURSE_BENCHMARK");
#endif

    std::vector<std::string> args{"Recurse", "--benchmark"};
    auto argv = toArgv(args);

    const auto prepared = recurse::prepareStartupArguments(static_cast<int>(argv.size()), argv.data());

    const std::vector<std::string> expected{
        "Recurse",
        "--profile_automation.autostart=true",
        "--profile_automation.skip_splash=true",
    };
    EXPECT_EQ(prepared.storage, expected);
}

TEST(RecurseBenchmarkStartupTest, BenchmarkFalseDisablesAutostartAndSkipSplash) {
    ScopedEnvVar benchmarkEnv("RECURSE_BENCHMARK");
#if defined(_WIN32)
    benchmarkEnv.set("");
#else
    unsetenv("RECURSE_BENCHMARK");
#endif

    std::vector<std::string> args{"Recurse", "--benchmark=false"};
    auto argv = toArgv(args);

    const auto prepared = recurse::prepareStartupArguments(static_cast<int>(argv.size()), argv.data());

    const std::vector<std::string> expected{
        "Recurse",
        "--profile_automation.autostart=false",
        "--profile_automation.skip_splash=false",
    };
    EXPECT_EQ(prepared.storage, expected);
}

TEST(RecurseBenchmarkStartupTest, EnvironmentOverridesInsertBeforeUserFlags) {
    ScopedEnvVar benchmarkEnv("RECURSE_BENCHMARK");
    benchmarkEnv.set("true");

    std::vector<std::string> args{"Recurse", "--profile_automation.skip_splash=false"};
    auto argv = toArgv(args);

    const auto prepared = recurse::prepareStartupArguments(static_cast<int>(argv.size()), argv.data());

    const std::vector<std::string> expected{
        "Recurse",
        "--profile_automation.autostart=true",
        "--profile_automation.skip_splash=true",
        "--profile_automation.skip_splash=false",
    };
    EXPECT_EQ(prepared.storage, expected);
}
