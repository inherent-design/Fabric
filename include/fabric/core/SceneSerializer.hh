#pragma once

#include "fabric/core/ECS.hh"
#include "fabric/core/FieldLayer.hh"
#include "fabric/core/Temporal.hh"
#include <array>
#include <cstdint>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <vector>

namespace fabric {

/// Physics shape type for serialization (mirrors Jolt shape kinds)
enum class PhysicsShapeType : uint8_t {
    Box,
    Sphere,
    Capsule,
    Mesh
};

/// Serializable physics body configuration (ECS component).
/// Stores shape/material config; Jolt runtime handles are reconstructed on load.
struct PhysicsBodyConfig {
    PhysicsShapeType shapeType = PhysicsShapeType::Box;
    float mass = 1.0f;
    float restitution = 0.3f;
    float friction = 0.5f;
    float velocityX = 0.0f;
    float velocityY = 0.0f;
    float velocityZ = 0.0f;
};

/// Serializable AI behavior tree configuration (ECS component).
/// BT runtime tree is reconstructed from btXmlId on load.
struct AIBehaviorConfig {
    std::string btXmlId;
    uint8_t currentState = 0; // Maps to AIState enum values
    std::vector<std::array<float, 3>> waypoints;
};

/// Serializable audio source configuration (ECS component).
/// miniaudio handles are reconstructed from soundPath on load.
struct AudioSourceConfig {
    std::string soundPath;
    float volume = 1.0f;
    bool looping = false;
    float positionX = 0.0f;
    float positionY = 0.0f;
    float positionZ = 0.0f;
};

/// Scene state descriptor for serialization
struct SceneConfig {
    std::vector<nlohmann::json> entities;
    std::vector<nlohmann::json> chunks;
    nlohmann::json timeline;
    std::optional<nlohmann::json> player;

    nlohmann::json toJson() const {
        nlohmann::json j;
        j["version"] = "1.0";
        j["entities"] = entities;
        j["chunks"] = chunks;
        j["timeline"] = timeline;
        if (player) {
            j["player"] = *player;
        }
        return j;
    }

    static SceneConfig fromJson(const nlohmann::json& j) {
        SceneConfig config;
        if (j.contains("version")) {
            config.entities = j.value("entities", std::vector<nlohmann::json>{});
            config.chunks = j.value("chunks", std::vector<nlohmann::json>{});
            config.timeline = j.value("timeline", nlohmann::json{});
            if (j.contains("player")) {
                config.player = j["player"];
            }
        }
        return config;
    }
};

/// Serializer for Fabric scene state including entities, chunks, timeline, and player state
class SceneSerializer {
  public:
    SceneSerializer() = default;

    /// Serialize complete scene to JSON
    /// Includes all entities, loaded chunks, timeline state, and optional player state
    nlohmann::json serialize(World& world, DensityField& density, EssenceField& essence, const Timeline& timeline,
                             const std::optional<Position>& playerPos = std::nullopt,
                             const std::optional<Position>& playerVel = std::nullopt);

    /// Serialize only entities (useful for partial saves)
    nlohmann::json serializeEntities(World& world);

    /// Serialize only chunk data (useful for terrain-only saves)
    nlohmann::json serializeChunks(DensityField& density, EssenceField& essence);

    /// Serialize timeline state
    nlohmann::json serializeTimeline(const Timeline& timeline);

    /// Deserialize JSON and restore scene state
    /// Returns true on success, false if JSON is invalid
    bool deserialize(const nlohmann::json& json, World& world, DensityField& density, EssenceField& essence,
                     Timeline& timeline, std::optional<Position>& playerPos, std::optional<Position>& playerVel);

    /// Deserialize only entities
    bool deserializeEntities(const nlohmann::json& json, World& world);

    /// Deserialize only chunks
    bool deserializeChunks(const nlohmann::json& json, DensityField& density, EssenceField& essence);

    /// Deserialize timeline state
    bool deserializeTimeline(const nlohmann::json& json, Timeline& timeline);

    /// Save JSON to file
    bool saveToFile(const std::string& filepath, const nlohmann::json& json);

    /// Load JSON from file
    std::optional<nlohmann::json> loadFromFile(const std::string& filepath);

  private:
    /// Serialize a single entity and its components
    nlohmann::json serializeEntity(flecs::entity entity);

    /// Serialize entity components
    nlohmann::json serializeComponents(flecs::entity entity);

    /// Create entity from JSON data
    flecs::entity createEntity(World& world, const nlohmann::json& entityJson);

    /// Restore entity components from JSON
    bool restoreComponents(flecs::entity entity, const nlohmann::json& componentsJson);

    /// Serialize TimeState to JSON
    nlohmann::json serializeTimeState(const TimeState& state);

    /// Deserialize TimeState from JSON
    TimeState deserializeTimeState(const nlohmann::json& json);
};

} // namespace fabric
