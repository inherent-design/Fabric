#include "fabric/core/SaveManager.hh"
#include "fabric/core/Log.hh"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace fabric {

static constexpr const char* kSaveVersion = "1.0";
static constexpr const char* kSaveExtension = ".json";

SaveManager::SaveManager(const std::string& saveDirectory) : saveDirectory_(saveDirectory) {
    std::error_code ec;
    std::filesystem::create_directories(saveDirectory_, ec);
    if (ec) {
        FABRIC_LOG_WARN("Failed to create save directory {}: {}", saveDirectory_, ec.message());
    }
}

bool SaveManager::save(const std::string& slotName, SceneSerializer& serializer, World& world, DensityField& density,
                       EssenceField& essence, Timeline& timeline, const std::optional<Position>& playerPos,
                       const std::optional<Position>& playerVel) {
    bool wasPaused = timeline.isPaused();
    if (!wasPaused) {
        timeline.pause();
    }

    nlohmann::json sceneData = serializer.serialize(world, density, essence, timeline, playerPos, playerVel);

    nlohmann::json envelope;
    envelope["save_version"] = kSaveVersion;
    envelope["slot"] = slotName;
    envelope["timestamp"] = currentTimestamp();
    envelope["scene"] = sceneData;

    std::string filepath = slotPath(slotName);
    bool ok = serializer.saveToFile(filepath, envelope);

    if (!wasPaused) {
        timeline.resume();
    }

    if (ok) {
        FABRIC_LOG_INFO("Saved slot '{}' to {}", slotName, filepath);
    } else {
        FABRIC_LOG_ERROR("Failed to save slot '{}'", slotName);
    }

    return ok;
}

bool SaveManager::load(const std::string& slotName, SceneSerializer& serializer, World& world, DensityField& density,
                       EssenceField& essence, Timeline& timeline, std::optional<Position>& playerPos,
                       std::optional<Position>& playerVel) {
    std::string filepath = slotPath(slotName);
    auto loaded = serializer.loadFromFile(filepath);
    if (!loaded) {
        FABRIC_LOG_ERROR("Failed to load slot '{}' from {}", slotName, filepath);
        return false;
    }

    const auto& envelope = *loaded;
    if (!envelope.contains("save_version")) {
        FABRIC_LOG_ERROR("Save file missing save_version field");
        return false;
    }

    std::string version = envelope["save_version"];
    if (version != kSaveVersion) {
        FABRIC_LOG_ERROR("Save version mismatch: expected '{}', got '{}'", kSaveVersion, version);
        return false;
    }

    if (!envelope.contains("scene")) {
        FABRIC_LOG_ERROR("Save file missing scene data");
        return false;
    }

    bool ok = serializer.deserialize(envelope["scene"], world, density, essence, timeline, playerPos, playerVel);
    if (ok) {
        FABRIC_LOG_INFO("Loaded slot '{}' from {}", slotName, filepath);
    } else {
        FABRIC_LOG_ERROR("Failed to deserialize slot '{}'", slotName);
    }

    return ok;
}

std::vector<SlotInfo> SaveManager::listSlots() const {
    std::vector<SlotInfo> slots;
    std::error_code ec;

    if (!std::filesystem::exists(saveDirectory_, ec) || !std::filesystem::is_directory(saveDirectory_, ec)) {
        return slots;
    }

    for (const auto& entry : std::filesystem::directory_iterator(saveDirectory_, ec)) {
        if (!entry.is_regular_file()) {
            continue;
        }

        if (entry.path().extension() != kSaveExtension) {
            continue;
        }

        try {
            std::ifstream file(entry.path());
            if (!file.is_open()) {
                continue;
            }

            nlohmann::json envelope;
            file >> envelope;

            SlotInfo info;
            info.name = entry.path().stem().string();
            info.timestamp = envelope.value("timestamp", "");
            info.version = envelope.value("save_version", "");
            info.sizeBytes = static_cast<size_t>(entry.file_size());
            slots.push_back(std::move(info));
        } catch (const std::exception& e) {
            FABRIC_LOG_WARN("Skipping unreadable save file {}: {}", entry.path().string(), e.what());
        }
    }

    return slots;
}

bool SaveManager::deleteSlot(const std::string& slotName) {
    std::string filepath = slotPath(slotName);
    std::error_code ec;
    bool removed = std::filesystem::remove(filepath, ec);
    if (!removed) {
        FABRIC_LOG_WARN("Failed to delete slot '{}': file not found or error", slotName);
    }
    return removed;
}

void SaveManager::enableAutosave(float intervalSeconds) {
    autosaveEnabled_ = true;
    autosaveInterval_ = intervalSeconds;
    autosaveTimer_ = 0.0f;
}

void SaveManager::tickAutosave(float dt, SceneSerializer& serializer, World& world, DensityField& density,
                               EssenceField& essence, Timeline& timeline, const std::optional<Position>& playerPos,
                               const std::optional<Position>& playerVel) {
    if (!autosaveEnabled_) {
        return;
    }

    autosaveTimer_ += dt;
    if (autosaveTimer_ < autosaveInterval_) {
        return;
    }

    autosaveTimer_ = 0.0f;

    std::string slotName = "autosave_" + std::to_string(autosaveIndex_);
    autosaveIndex_ = (autosaveIndex_ + 1) % 2;

    save(slotName, serializer, world, density, essence, timeline, playerPos, playerVel);
}

std::string SaveManager::slotPath(const std::string& slotName) const {
    return (std::filesystem::path(saveDirectory_) / (slotName + kSaveExtension)).string();
}

std::string SaveManager::currentTimestamp() const {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::ostringstream oss;
    oss << std::put_time(std::gmtime(&time), "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}

} // namespace fabric
