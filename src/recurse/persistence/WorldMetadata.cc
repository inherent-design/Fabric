#include "recurse/persistence/WorldMetadata.hh"

#include "fabric/utils/ErrorHandling.hh"
#include <chrono>
#include <fstream>
#include <iomanip>
#include <random>
#include <sstream>
#include <toml++/toml.hpp>

namespace recurse {

namespace {

WorldType worldTypeFromString(std::string_view str) {
    if (str == "Natural")
        return WorldType::Natural;
    if (str == "Flat")
        return WorldType::Flat;
    fabric::throwError("Unknown WorldType: " + std::string(str));
}

std::string_view worldTypeToString(WorldType type) {
    switch (type) {
        case WorldType::Flat:
            return "Flat";
        case WorldType::Natural:
            return "Natural";
    }
    return "Flat";
}

} // namespace

WorldMetadata WorldMetadata::fromTOML(const std::string& path) {
    auto tbl = toml::parse_file(path);

    WorldMetadata meta;
    meta.uuid = tbl["uuid"].value_or(std::string{});
    meta.name = tbl["name"].value_or(std::string{"Unnamed"});
    meta.type = worldTypeFromString(tbl["type"].value_or(std::string_view{"Flat"}));
    meta.seed = tbl["seed"].value_or(int64_t{0});
    meta.createdAt = tbl["created_at"].value_or(std::string{});
    meta.lastPlayed = tbl["last_played"].value_or(std::string{});

    if (meta.uuid.empty()) {
        fabric::throwError("world.toml missing uuid: " + path);
    }

    return meta;
}

void WorldMetadata::toTOML(const std::string& path) const {
    auto tbl = toml::table{
        {"uuid", uuid},
        {"name", name},
        {"type", std::string(worldTypeToString(type))},
        {"seed", seed},
        {"created_at", createdAt},
        {"last_played", lastPlayed},
    };

    std::ofstream out(path);
    if (!out.is_open()) {
        fabric::throwError("Failed to open world.toml for writing: " + path);
    }
    out << tbl;
}

std::string WorldMetadata::generateUUID() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<uint32_t> dist;
    uint32_t val = dist(gen);
    std::ostringstream ss;
    ss << std::hex << std::setfill('0') << std::setw(8) << val;
    return ss.str();
}

std::string WorldMetadata::nowISO8601() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#if defined(_WIN32)
    gmtime_s(&tm, &time);
#else
    gmtime_r(&time, &tm);
#endif
    std::ostringstream ss;
    ss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
    return ss.str();
}

} // namespace recurse
