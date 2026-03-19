#include "recurse/BenchmarkStartup.hh"

#include <cstdlib>
#include <optional>
#include <string_view>

namespace recurse {

namespace {

constexpr std::string_view K_BENCHMARK_FLAG = "--benchmark";
constexpr std::string_view K_BENCHMARK_FLAG_PREFIX = "--benchmark=";
constexpr std::string_view K_BENCHMARK_ENV = "RECURSE_BENCHMARK";
constexpr std::string_view K_AUTOSTART_TRUE = "--profile_automation.autostart=true";
constexpr std::string_view K_AUTOSTART_FALSE = "--profile_automation.autostart=false";
constexpr std::string_view K_SKIP_SPLASH_TRUE = "--profile_automation.skip_splash=true";
constexpr std::string_view K_SKIP_SPLASH_FALSE = "--profile_automation.skip_splash=false";

std::optional<bool> parseBenchmarkToggle(std::string_view raw) {
    if (raw == "1" || raw == "true" || raw == "on" || raw == "yes")
        return true;
    if (raw == "0" || raw == "false" || raw == "off" || raw == "no")
        return false;
    return std::nullopt;
}

std::optional<bool> parseBenchmarkArgument(std::string_view arg) {
    if (arg == K_BENCHMARK_FLAG)
        return true;
    if (!arg.starts_with(K_BENCHMARK_FLAG_PREFIX))
        return std::nullopt;
    return parseBenchmarkToggle(arg.substr(K_BENCHMARK_FLAG_PREFIX.size()));
}

void appendBenchmarkOverrides(std::vector<std::string>& storage, bool enabled) {
    storage.emplace_back(enabled ? K_AUTOSTART_TRUE : K_AUTOSTART_FALSE);
    storage.emplace_back(enabled ? K_SKIP_SPLASH_TRUE : K_SKIP_SPLASH_FALSE);
}

} // namespace

PreparedStartupArguments prepareStartupArguments(int argc, char* argv[]) {
    PreparedStartupArguments prepared;
    prepared.storage.reserve(static_cast<size_t>(argc) + 4);

    prepared.storage.emplace_back((argc > 0 && argv && argv[0]) ? argv[0] : "Recurse");

    if (const char* envValue = std::getenv(K_BENCHMARK_ENV.data())) {
        if (auto enabled = parseBenchmarkToggle(envValue); enabled.has_value())
            appendBenchmarkOverrides(prepared.storage, *enabled);
    }

    for (int i = 1; i < argc; ++i) {
        const std::string_view arg = (argv && argv[i]) ? std::string_view(argv[i]) : std::string_view{};
        if (auto enabled = parseBenchmarkArgument(arg); enabled.has_value()) {
            appendBenchmarkOverrides(prepared.storage, *enabled);
            continue;
        }
        prepared.storage.emplace_back(arg);
    }

    prepared.argv.reserve(prepared.storage.size());
    for (auto& arg : prepared.storage)
        prepared.argv.push_back(arg.data());

    return prepared;
}

} // namespace recurse
