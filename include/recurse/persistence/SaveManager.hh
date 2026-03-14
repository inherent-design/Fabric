#pragma once

#include "fabric/core/Spatial.hh"
#include "fabric/core/Temporal.hh"
#include "fabric/ecs/ECS.hh"
#include "recurse/persistence/SceneSerializer.hh"
#include <optional>
#include <string>
#include <vector>

namespace recurse {

using fabric::Position;
using fabric::Timeline;
using fabric::World;

/// Metadata for a save slot on disk
struct SlotInfo {
    std::string name;
    std::string timestamp;
    std::string version;
    size_t sizeBytes = 0;
};

/// @deprecated Chunk persistence replaced by ChunkStore + ChunkSaveService (WS-4).
/// SaveManager remains for entity/timeline serialization until that migrates.
/// Orchestrates full game state capture/restore through named save slots.
/// Wraps SceneSerializer for the actual serialization and adds a metadata
/// envelope (version, timestamp, slot name) around each save file.
class SaveManager {
  public:
    explicit SaveManager(const std::string& saveDirectory);

    /// Serialize complete game state into saves/<slotName>.json.
    /// Pauses the timeline during serialization and resumes afterward.
    bool save(const std::string& slotName, SceneSerializer& serializer, World& world, Timeline& timeline,
              const std::optional<Position>& playerPos, const std::optional<Position>& playerVel);

    /// Load game state from saves/<slotName>.json.
    /// Validates save_version before deserializing. Resumes timeline on success.
    bool load(const std::string& slotName, SceneSerializer& serializer, World& world, Timeline& timeline,
              std::optional<Position>& playerPos, std::optional<Position>& playerVel);

    /// Scan the save directory and return metadata for each slot found.
    std::vector<SlotInfo> listSlots() const;

    /// Remove the save file for the given slot.
    bool deleteSlot(const std::string& slotName);

    /// Enable rotating autosave between autosave_0 and autosave_1.
    void enableAutosave(float intervalSeconds = 300.0f);

    /// Reset per-world timer and index state. Does not clear saveDirectory_.
    void resetWorldState();

    /// Call each frame (or fixed tick). Decrements the autosave timer and
    /// triggers a save when the interval elapses.
    void tickAutosave(float dt, SceneSerializer& serializer, World& world, Timeline& timeline,
                      const std::optional<Position>& playerPos, const std::optional<Position>& playerVel);

  private:
    std::string slotPath(const std::string& slotName) const;
    std::string currentTimestamp() const;

    std::string saveDirectory_;
    bool autosaveEnabled_ = false;
    float autosaveInterval_ = 300.0f;
    float autosaveTimer_ = 0.0f;
    int autosaveIndex_ = 0;
};

} // namespace recurse
